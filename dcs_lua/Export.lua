-- MyMCU Export script for use with DCS client.
-- See https://github.com/arron-h/mymcu for more details


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

local lastCaution = 255
local lastFireWarning = 255
local lastAutopilot = 255

function LuaExportStart()
-- Works once just before mission start.
-- Make initializations of your files or connections here.
  package.path  = package.path..";.\\LuaSocket\\?.lua" .. ";.\\Scripts\\?.lua"
  package.cpath = package.cpath..";.\\LuaSocket\\?.dll"
  socket = require("socket")
  senderConn = socket.udp()
  senderConn:settimeout(0)
  
  recvConn = socket.udp()
  recvConn:setsockname("*", 7778)
  recvConn:settimeout(0)

end

function LuaExportStop()
-- Works once just after mission stop.
-- Close files and/or connections here.
	senderConn:close()
end

function FastBytesToString(bytes)
  s = {}
  for i = 1, #bytes do
    s[i] = string.char(bytes[i])
  end
  return table.concat(s)
end

function UdpSend(data)
    socket.try(senderConn:sendto(FastBytesToString(data), "239.255.50.10", 5010))
end

function LuaExportAfterNextFrame()
    -- Works just after every simulation frame.
	local mainPanel = GetDevice(0)

    -- Master warning
    if mainPanel:get_argument_value(13) < 0.5 then
        masterCaution = 0
    else
        masterCaution = 1
    end
    if (masterCaution ~= lastCaution) then
		if (masterCaution == 1) then
			UdpSend({MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_ANNUC_CHANGED, MYMCU_ANNUC_CAUT_ON})
		else
			UdpSend({MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_ANNUC_CHANGED, MYMCU_ANNUC_CAUT_OFF})
		end
		lastCaution = masterCaution
	end
    
    -- Fire warning
    if mainPanel:get_argument_value(10) > 0.5 or mainPanel:get_argument_value(11) > 0.5 then
        fireWarning = 1
    else
        fireWarning = 0
    end
    if (fireWarning ~= lastFireWarning) then
		if (fireWarning == 1) then
			UdpSend({MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_ANNUC_CHANGED, MYMCU_ANNUC_WARN_ON})
		else
			UdpSend({MYMCU_HEADER_A, MYMCU_HEADER_B, MYMCU_CMDS_ANNUC_CHANGED, MYMCU_ANNUC_WARN_OFF})
		end
		lastFireWarning = fireWarning
	end
end

local function cap(value, limits, cycle)
	if cycle then
		if value < limits[1] then return limits[2] end
		if value > limits[2] then return limits[1] end
	else
		if value <= limits[1] then return limits[1] end
		if value >= limits[2] then return limits[2] end
	end
	return value
end

function process_rotary(device, readId, writeId, invert, dir, pulses)
    local dev = GetDevice(device)
    local limits = {0.0, 1.0}
    local intervalLength = limits[2] - limits[1]
    local newValue = ((GetDevice(0):get_argument_value(readId) - limits[1]) / intervalLength) * 65535
    
    if dir == 0 then -- CW
        newValue = 65535
    else
        newValue = 0
    end
    dev:performClickableAction(writeId, newValue/65535*intervalLength + limits[1])
end

function LuaExportBeforeNextFrame()
    local rxbuf = "";
    local lInput = nil
    while true do
		lInput = recvConn:receive()
		if not lInput then break end
		rxbuf = rxbuf .. lInput
	end

    if (#rxbuf > 0) then
        local t = {}
        for i = 1, #rxbuf do
            t[i] = string.byte(rxbuf:sub(i, i))
        end
        
        if (t[3] == MYMCU_CMDS_ROTARY_CHANGED) then
            local rotaryId = t[4]
            if (rotaryId == MYMCU_ROTARY_COM_INNER) then
                -- COMM1
                process_rotary(25, 124, 3033, true, t[5], t[6])
            elseif (rotaryId == MYMCU_ROTARY_NAV_INNER) then
                -- COMM2
                process_rotary(25, 126, 3034, true, t[5], t[6])
            elseif (rotaryId == MYMCU_ROTARY_NAV_INNER) then
                -- Radar altimeter
                process_rotary(30, 291, 3002, true, t[5], t[6])
            end 
         end
    end
end
