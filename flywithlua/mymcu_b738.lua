-- Data Refs
joystick_buttons = dataref_table("sim/joystick/joystick_button_values")
master_caution_ref = dataref_table("laminar/B738/annunciator/master_caution_light")
master_warning_ref = dataref_table("laminar/B738/annunciator/fire_bell_annun")
dataref("ap_cmdA_ref", "laminar/B738/autopilot/cmd_a_status", "readable")
dataref("ap_cmdB_ref", "laminar/B738/autopilot/cmd_b_status", "readable")

-- HID setup
local hid_device_path = ""
for i=1,NUMBER_OF_HID_DEVICES do
	if (ALL_HID_DEVICES[i].vendor_id == 0x16C0 and
			ALL_HID_DEVICES[i].product_id == 0x0482 and
			ALL_HID_DEVICES[i].usage_page == 0xFFAB and
			ALL_HID_DEVICES[i].usage == 0x02AB) then
			
		hid_device_path = ALL_HID_DEVICES[i].path
		logMsg("Found usable HID device (index = " .. tostring(i) .. ")")
		break
	end
end

local hid_device = nil
if (hid_device_path ~= "") then
	logMsg("Attempting to open HID device")
	hid_device = hid_open_path(hid_device_path)
	if (not hid_device) then
		logMsg("Could not open HID device")
	else
		hid_set_nonblocking(hid_device, 1)
	end
else
	logMsg("Could not find any usable HID device")
end


-- MyMCU defs
local MYMCU_MAX_MSG_SIZE = 32
-- Header
local MYMCU_HEADER_A = 0xFC
local MYMCU_HEADER_B = 0xAB
-- Commands
local MYMCU_CMDS_ANNUC_CHANGED   = 0x1
local MYMCU_CMDS_COM_ACT_CHANGED = 0x2
local MYMCU_CMDS_COM_SBY_CHANGED = 0x3
local MYMCU_CMDS_NAV_ACT_CHANGED = 0x4
local MYMCU_CMDS_NAV_SBY_CHANGED = 0x5
local MYMCU_CMDS_LCD_ON          = 0x6
local MYMCU_CMDS_LCD_OFF         = 0x7
local MYMCU_CMDS_HEARTBEAT       = 0x8
local MYMCU_CMDS_LCD_STRING      = 0x9
local MYMCU_CMDS_ROTARY_CHANGED  = 0xA
-- Annunciator
local MYMCU_ANNUC_CAUT_ON  = 0x1
local MYMCU_ANNUC_CAUT_OFF = 0x2
local MYMCU_ANNUC_WARN_ON  = 0x4
local MYMCU_ANNUC_WARN_OFF = 0x8
local MYMCU_ANNUC_AP_ON  = 0x10
local MYMCU_ANNUC_AP_OFF = 0x20
local MYMCU_ANNUC_YD_ON  = 0x40
local MYMCU_ANNUC_YD_OFF = 0x80
-- Rotary directions
local MYMCU_ROTARYDIR_INC = 0
local MYMCU_ROTARYDIR_DEC = 1
-- Rotary definitions
local MYMCU_ROTARY_COM_INNER = 1
local MYMCU_ROTARY_COM_OUTER = 2
local MYMCU_ROTARY_NAV_INNER = 3
local MYMCU_ROTARY_NAV_OUTER = 4
local MYMCU_ROTARY_FMS_INNER = 5
local MYMCU_ROTARY_FMS_OUTER = 6
local MYMCU_ROTARY_HDG = 7
local MYMCU_ROTARY_CRS = 8
local MYMCU_ROTARY_ALT = 9
local MYMCU_ROTARY_UPDN = 10

local lastCaution = -1
local lastWarning = -1
local lastAutopilotA = -1
local lastAutopilotB = -1

function process_annunciators()
	-------------------------------
	-- CHECK CAUTION/FIREBELL LIGHTS
	local newCaution = master_caution_ref[0]
	local newWarning = master_warning_ref[0]
	if (newCaution ~= lastCaution) then
		if (newCaution == 1) then
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_ANNUC_CHANGED, MYMCU_ANNUC_CAUT_ON)
		else
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_ANNUC_CHANGED, MYMCU_ANNUC_CAUT_OFF)
		end
		lastCaution = newCaution
	end
	if (newWarning ~= lastWarning) then
		if (newWarning == 1) then
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_ANNUC_CHANGED, MYMCU_ANNUC_WARN_ON)
		else
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_ANNUC_CHANGED, MYMCU_ANNUC_WARN_OFF)
		end
		lastWarning = newWarning
	end
	-------------------------------
	-- CHECK AP CMD A/B LIGHTS
	local newApA = ap_cmdA_ref
	local newApB = ap_cmdB_ref
	if (newApA ~= lastAutopilotA) then
		if (newApA == 1) then
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_ANNUC_CHANGED, MYMCU_ANNUC_AP_ON)
		else
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_ANNUC_CHANGED, MYMCU_ANNUC_AP_OFF)
		end
		lastAutopilotA = newApA
	end
	if (newApB ~= lastAutopilotB) then
		if (newApB == 1) then
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_ANNUC_CHANGED, MYMCU_ANNUC_YD_ON)
		else
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_ANNUC_CHANGED, MYMCU_ANNUC_YD_OFF)
		end
		lastAutopilotB = newApB
	end
end

function process_dataref_rotary(dataref, rotaryDir, rotaryPulses, multiplier)
	local dir = 0
	if (rotaryDir == MYMCU_ROTARYDIR_INC) then
		dir = 1 * multiplier
	elseif (rotaryDir == MYMCU_ROTARYDIR_DEC) then
		dir = -1 * multiplier
	end
	
	dataref[0] = dataref[0] + (dir * (rotaryPulses*rotaryPulses))
end

function process_command_rotary(command_base, rotaryDir, rotaryPulses, short, finalSuffix)
	local suffix = ""
	if (rotaryDir == MYMCU_ROTARYDIR_INC) then
		suffix = "_up"
	elseif (rotaryDir == MYMCU_ROTARYDIR_DEC) then
        if (short == true) then
            suffix = "_dn"
        else
            suffix = "_down"
        end
	end
        
    if (finalSuffix) then
        suffix = suffix .. "_" .. finalSuffix
    end
    
	for i=1,rotaryPulses*rotaryPulses do
		command_once(command_base .. suffix)
	end
end

function process_rotarys()
	numVals, headerA, headerB, cmd, rotaryId, rotaryDir, rotaryPulses = hid_read(hid_device, 6)
	if (numVals ~= nil and numVals > 0) then
		if (cmd == MYMCU_CMDS_ROTARY_CHANGED) then
			logMsg("Incoming rotary command... id=" .. tostring(rotaryId) .. " dir=" .. tostring(rotaryDir) .. " pulses=" .. tostring(rotaryPulses))
			if (rotaryId == MYMCU_ROTARY_COM_INNER) then
				process_command_rotary("sim/radios/stby_com1_fine", rotaryDir, rotaryPulses, false, "833")
			elseif (rotaryId == MYMCU_ROTARY_COM_OUTER) then
				process_command_rotary("sim/radios/stby_com1_coarse", rotaryDir, rotaryPulses, false)
			elseif (rotaryId == MYMCU_ROTARY_FMS_INNER) then
				process_command_rotary("laminar/B738/pilot/barometer", rotaryDir, rotaryPulses, false)
			elseif (rotaryId == MYMCU_ROTARY_FMS_OUTER) then
				process_command_rotary("sim/autopilot/airspeed", rotaryDir, rotaryPulses)
			elseif (rotaryId == MYMCU_ROTARY_HDG) then
				process_command_rotary("laminar/B738/autopilot/heading", rotaryDir, rotaryPulses, true)
			elseif (rotaryId == MYMCU_ROTARY_CRS) then
				process_command_rotary("laminar/B738/autopilot/course_pilot", rotaryDir, rotaryPulses, true)
			elseif (rotaryId == MYMCU_ROTARY_ALT) then
				process_command_rotary("laminar/B738/autopilot/altitude", rotaryDir, rotaryPulses, true)
			elseif (rotaryId == MYMCU_ROTARY_UPDN) then
				process_command_rotary("sim/autopilot/vertical_speed", rotaryDir, rotaryPulses, false)
			end
		end
	end
end

local clockLastHeartbeat = os.clock()
function send_heartbeat()
	local clockNow = os.clock()
	if (clockNow - clockLastHeartbeat > 2.0) then
		hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_HEARTBEAT)
		clockLastHeartbeat = clockNow
	end
end

function process_b738()
	local clockNow = os.clock()
	
	-------------------------------
	-- HID connection
	-- Only process if we have a valid HID device connected
	if (hid_device) then
		process_annunciators()
		process_rotarys()
		send_heartbeat()
	end
end

do_every_frame("process_b738()")
