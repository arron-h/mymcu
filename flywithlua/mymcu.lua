if (PLANE_ICAO == "TBM9") then
	dofile(SCRIPT_DIRECTORY .. "MyMCU_Aircraft\\mymcu_tbm900.lua")
elseif (PLANE_ICAO == "P28A") then
	dofile(SCRIPT_DIRECTORY .. "MyMCU_Aircraft\\mymcu_pa28.lua")
elseif (PLANE_ICAO == "B738") then
	dofile(SCRIPT_DIRECTORY .. "MyMCU_Aircraft\\mymcu_b738.lua")
end
