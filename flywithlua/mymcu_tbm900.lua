-- Data Refs
joystick_axis = dataref_table("sim/joystick/joystick_axis_values") -- Table of all Joystick AXIS values
master_caution_ref = dataref_table("tbm900/lights/cas/master_caut")
master_warning_ref = dataref_table("tbm900/lights/cas/master_warn")
com1_act_freq_ref = dataref_table("sim/cockpit2/radios/actuators/com1_frequency_hz_833")
com1_sby_freq_ref = dataref_table("sim/cockpit2/radios/actuators/com1_standby_frequency_hz_833")
nav1_act_freq_ref = dataref_table("sim/cockpit2/radios/actuators/nav1_frequency_hz")
nav1_sby_freq_ref = dataref_table("sim/cockpit2/radios/actuators/nav1_standby_frequency_hz")
crash_bar_ref = dataref_table("tbm900/switches/elec/emerg_handle")
local mixture_axis = 27  -- Axis ID

-- Initial vals
local old_axis = joystick_axis[mixture_axis]

function in_top_range(axis)
	if (axis > 0.05 and axis < 0.45) then
		return true
	end
	return false
end

local hid_device_path = ""
for i=1,NUMBER_OF_HID_DEVICES do
	if (ALL_HID_DEVICES[i].vendor_id == 0x16C0 and
			ALL_HID_DEVICES[i].product_id == 0x0482 and
			ALL_HID_DEVICES[i].usage_page == 0xffab and
			ALL_HID_DEVICES[i].usage == 0x02ab) then
			
		hid_device_path = ALL_HID_DEVICES[i].path
		logMsg("Found usable HID device")
		break
	end
end

local hid_device = 0
if (hid_device_path ~= "") then
	logMsg("Attempting to open HID device")
	hid_device = hid_open_path(hid_device_path)
	if (not hid_device) then
		logMsg("Could not open HID device")
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
-- Annunciator
local MYMCU_ANNUC_CAUT_ON  = 0x1
local MYMUC_ANNUC_CAUT_OFF = 0x2
local MYMCU_ANNUC_WARN_ON  = 0x4
local MYMUC_ANNUC_WARN_OFF = 0x8

-- Definitions/constants
local AVIONICS_INIT_TIMEOUT_S = 5.0
local AVIONICS_ON_TIMEOUT_S   = 20.0

function lcd_string(message)
	local packedVals = {}
	local strLen = string.len(message)
	
	if (strLen + 3 > MYMCU_MAX_MSG_SIZE) then -- 3 = HEADER (2 bytes) + CMD (1 byte)
		logMsg("MyMCU: ERROR: lcd_string too large!")
		return
	end

	-- String length first
	table.insert(packedVals, strLen)
	for idx=1,strLen do
		table.insert(packedVals, string.byte(message, idx))
	end
	
	-- NUL terminate
	table.insert(packedVals, 0)

	return unpack(packedVals)
end

local lastCaution = -1
local lastWarning = -1
function process_annunciators()
	-------------------------------
	-- CHECK CAUTION/WARNING LIGHTS
	local newCaution = master_caution_ref[0]
	local newWarning = master_warning_ref[0]
	if (newCaution ~= lastCaution) then
		if (newCaution == 1) then
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_ANNUC_CHANGED, MYMCU_ANNUC_CAUT_ON)
		else
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_ANNUC_CHANGED, MYMUC_ANNUC_CAUT_OFF)
		end
		lastCaution = newCaution
	end
	if (newWarning ~= lastWarning) then
		if (newWarning == 1) then
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_ANNUC_CHANGED, MYMCU_ANNUC_WARN_ON)
		else
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_ANNUC_CHANGED, MYMUC_ANNUC_WARN_OFF)
		end
		lastWarning = newWarning
	end
end

function get_radio_components(freqHz, khzUnits)
	local stringVal = tostring(freqHz)
	local packedVals = {}
	local offset = 0
	local parts  = 3 -- always 3 Mhz units
	for idx=1,2 do
		for i=1+offset, parts+offset do
			local part = tonumber(string.sub(stringVal, i, i))
			table.insert(packedVals, 48 + part)
		end
		-- insert decimal
		table.insert(packedVals, 46)
		offset = offset + 3
		parts  = khzUnits
	end

	return unpack(packedVals)
end

local AVSTATE_OFF  = 0
local AVSTATE_PWR  = 1
local AVSTATE_INIT = 2
local AVSTATE_ON   = 3

local avionicsState = AVSTATE_OFF
local avionicsOnClock = -1
local lastCrashBarPos = -1
local crashBarUpTime = -1
local lastCOM1ACT = -1
local lastCOM1SBY = -1
local lastNAV1ACT = -1
local lastNAV1SBY = -1
function process_radios()
	local clockNow = os.clock()
	-------------------------------
	-- AVIONICS STATE
	local newCrashParPos = math.floor(crash_bar_ref[0]) -- ref is a float
	if (newCrashParPos ~= lastCrashBarPos) then
		lastCrashBarPos = newCrashParPos
		if (newCrashParPos == 1) then
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_LCD_OFF)
			avionicsState = AVSTATE_OFF
		else
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_LCD_ON)
			avionicsState = AVSTATE_PWR
			crashBarUpTime = clockNow
			
			-- Reset frequencies
			lastCOM1ACT = -1
			lastCOM1SBY = -1
			lastNAV1ACT = -1
			lastNAV1SBY = -1
		end
	end
	
	-- Move from PWR to INIT phase after timeout
	if (avionicsState == AVSTATE_PWR and (clockNow - crashBarUpTime > AVIONICS_INIT_TIMEOUT_S)) then
		hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_LCD_STRING, lcd_string("INITIALIZING..."))
		avionicsState = AVSTATE_INIT
	end

	-- Move from INIT to ON phase after timeout
	if (avionicsState == AVSTATE_INIT and (clockNow - crashBarUpTime > AVIONICS_ON_TIMEOUT_S)) then
		avionicsState = AVSTATE_ON
	end
	
	if (avionicsState == AVSTATE_ON) then
		-------------------------------
		-- COM1 ACTIVE
		local newCOM1ACT = com1_act_freq_ref[0]
		if (newCOM1ACT ~= lastCOM1ACT) then
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_COM_ACT_CHANGED, get_radio_components(newCOM1ACT, 3))
			lastCOM1ACT = newCOM1ACT;
		end
		-------------------------------
		-- COM1 STANDBY
		local newCOM1SBY = com1_sby_freq_ref[0]
		if (newCOM1SBY ~= lastCOM1SBY) then
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_COM_SBY_CHANGED, get_radio_components(newCOM1SBY, 3))
			lastCOM1SBY = newCOM1SBY;
		end
		-------------------------------
		-- NAV1 ACTIVE
		local newNAV1ACT = nav1_act_freq_ref[0]
		if (newNAV1ACT ~= lastNAV1ACT) then
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_NAV_ACT_CHANGED, get_radio_components(newNAV1ACT, 2))
			lastNAV1ACT = newNAV1ACT;
		end
		-------------------------------
		-- NAV1 STANDBY
		local newNAV1SBY = nav1_sby_freq_ref[0]
		if (newNAV1SBY ~= lastNAV1SBY) then
			hid_write(hid_device, 0, MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_NAV_SBY_CHANGED, get_radio_components(newNAV1SBY, 2))
			lastNAV1SBY = newNAV1SBY;
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

function process_tbm()
	-------------------------------
	-- CONDITION LEVER
	local new_axis = joystick_axis[mixture_axis]
	if (new_axis ~= old_axis) then
		if ((new_axis > old_axis and new_axis - old_axis > .25) or new_axis == 1) then
			command_once("sim/engines/mixture_down")
			old_axis = new_axis
		elseif ((new_axis < old_axis and old_axis - new_axis > .25 and not in_top_range(new_axis)) or new_axis == 0) then
			command_once("sim/engines/mixture_up")
			old_axis = new_axis
		end
	end
	
	-------------------------------
	-- HID connection
	-- Only process if we have a valid HID device connected
	if (hid_device) then
		process_annunciators()
		process_radios()
		send_heartbeat()
	end
end

if (PLANE_ICAO == "TBM9") then
	do_every_frame("process_tbm()")
end