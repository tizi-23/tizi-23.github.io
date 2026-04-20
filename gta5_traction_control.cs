using System;
using System.Windows.Forms;
using GTA;
using GTA.Native;
using GTA.Math;

public class TractionControl : Script
{
    // Config
    private bool tcEnabled = true;
    private float tcStrength = 0.7f; // 0.0 = no TC, 1.0 = max TC
    private Keys toggleKey = Keys.F8;
    private Keys increaseKey = Keys.OemCloseBrackets; // ]
    private Keys decreaseKey = Keys.OemOpenBrackets; // [
    
    // State tracking
    private bool wasTogglePressed = false;
    private bool wasIncreasePressed = false;
    private bool wasDecreasePressed = false;
    private int notificationHandle = 0;
    
    // Performance cache
    private Vehicle lastVehicle = null;
    private float lastWheelSpeed = 0f;
    private int skipFrames = 0;
    
    public TractionControl()
    {
        Tick += OnTick;
        KeyDown += OnKeyDown;
        
        ShowNotification("~g~Traction Control Loaded~w~\nToggle: " + toggleKey + 
                        "\nAdjust: " + decreaseKey + "/" + increaseKey);
    }
    
    private void OnKeyDown(object sender, KeyEventArgs e)
    {
        // Allow runtime keybind changes via ini or menu
        // For now, keys are set in code above
    }
    
    private void OnTick(object sender, EventArgs e)
    {
        Ped player = Game.Player.Character;
        
        if (!player.IsInVehicle())
        {
            lastVehicle = null;
            return;
        }
        
        Vehicle veh = player.CurrentVehicle;
        
        // Only apply to cars (exclude bikes, tanks, boats, planes, etc)
        if (veh.Model.IsBike || veh.Model.IsBoat || veh.Model.IsHelicopter || 
            veh.Model.IsPlane || veh.Model.IsTrain)
        {
            lastVehicle = null;
            return;
        }
        
        // Performance optimization: skip frames for non-sports cars
        if (lastVehicle != veh)
        {
            lastVehicle = veh;
            skipFrames = IsSportsCar(veh) ? 0 : 2; // Process sports cars every frame
        }
        
        if (skipFrames > 0 && Game.GameTime % (skipFrames + 1) != 0)
            return;
        
        // Handle keybinds
        HandleInput();
        
        // Apply traction control if enabled
        if (tcEnabled && player == veh.Driver)
        {
            ApplyTractionControl(veh);
        }
    }
    
    private void HandleInput()
    {
        bool togglePressed = Game.IsKeyPressed(toggleKey);
        bool increasePressed = Game.IsKeyPressed(increaseKey);
        bool decreasePressed = Game.IsKeyPressed(decreaseKey);
        
        // Toggle TC on/off
        if (togglePressed && !wasTogglePressed)
        {
            tcEnabled = !tcEnabled;
            ShowNotification(tcEnabled ? 
                "~g~TC ON~w~ (" + (int)(tcStrength * 100) + "%)" : 
                "~r~TC OFF");
        }
        wasTogglePressed = togglePressed;
        
        // Increase TC strength
        if (increasePressed && !wasIncreasePressed && tcEnabled)
        {
            tcStrength = Math.Min(1.0f, tcStrength + 0.1f);
            ShowNotification("TC: ~b~" + (int)(tcStrength * 100) + "%");
        }
        wasIncreasePressed = increasePressed;
        
        // Decrease TC strength
        if (decreasePressed && !wasDecreasePressed && tcEnabled)
        {
            tcStrength = Math.Max(0.1f, tcStrength - 0.1f);
            ShowNotification("TC: ~b~" + (int)(tcStrength * 100) + "%");
        }
        wasDecreasePressed = decreasePressed;
    }
    
    private void ApplyTractionControl(Vehicle veh)
    {
        float throttle = Game.GetControlValueNormalized(0, Control.VehicleAccelerate);
        float brake = Game.GetControlValueNormalized(0, Control.VehicleBrake);
        
        // Only apply TC during acceleration
        if (throttle < 0.1f) return;
        
        // Get wheel speeds and detect slip
        float wheelSpeed = veh.Speed;
        float acceleration = wheelSpeed - lastWheelSpeed;
        lastWheelSpeed = wheelSpeed;
        
        // Get vehicle grip/traction
        float grip = GetVehicleGrip(veh);
        float slipThreshold = 0.5f + (grip * 0.5f); // Higher grip = higher threshold
        
        // Detect wheel spin (rapid acceleration with low grip)
        bool isSlipping = acceleration > slipThreshold && grip < 0.8f;
        
        if (isSlipping)
        {
            // Calculate TC intervention
            float intervention = tcStrength * (1.0f - grip);
            
            // Reduce engine power by disabling throttle input temporarily
            Game.DisableControlThisFrame(0, Control.VehicleAccelerate);
            
            // Apply reduced throttle
            float reducedThrottle = throttle * (1.0f - (intervention * 0.8f));
            Function.Call(Hash.SET_VEHICLE_FORWARD_SPEED, veh, 
                         veh.Speed * (1.0f - (intervention * 0.15f)));
            
            // Apply slight brake force for aggressive TC settings
            if (tcStrength > 0.7f)
            {
                float brakeForce = (tcStrength - 0.7f) * 0.3f * intervention;
                Function.Call(Hash.SET_VEHICLE_BRAKE_LIGHTS, veh, true);
                veh.Speed = veh.Speed * (1.0f - brakeForce);
            }
        }
    }
    
    private float GetVehicleGrip(Vehicle veh)
    {
        // Calculate grip based on multiple factors
        float baseGrip = 1.0f;
        
        // Surface detection
        Vector3 pos = veh.Position;
        pos.Z -= 1.5f;
        
        int materialHash = Function.Call<int>(Hash._GET_MATERIAL_AT_COORDINATES, 
                                              pos.X, pos.Y, pos.Z);
        
        // Reduce grip on slippery surfaces
        if (materialHash == 0 || IsSlipperySurface(materialHash))
            baseGrip *= 0.6f;
        
        // Weather effects
        if (Function.Call<int>(Hash.GET_PREV_WEATHER_TYPE_HASH_NAME) == 
            Game.GenerateHash("RAIN") || 
            Game.Weather == Weather.Raining)
            baseGrip *= 0.75f;
        
        // Speed factor - less grip at higher speeds
        float speedFactor = Math.Max(0.5f, 1.0f - (veh.Speed / 100.0f));
        baseGrip *= speedFactor;
        
        // Wheel health
        float wheelHealth = 1.0f;
        for (int i = 0; i < veh.Wheels.Count; i++)
        {
            if (Function.Call<bool>(Hash.IS_VEHICLE_TYRE_BURST, veh, i, false))
                wheelHealth *= 0.3f;
        }
        baseGrip *= wheelHealth;
        
        return Math.Max(0.1f, Math.Min(1.0f, baseGrip));
    }
    
    private bool IsSportsCar(Vehicle veh)
    {
        VehicleClass vClass = veh.ClassType;
        return vClass == VehicleClass.Super || 
               vClass == VehicleClass.Sports || 
               vClass == VehicleClass.SportsClassics;
    }
    
    private bool IsSlipperySurface(int materialHash)
    {
        // Common slippery material hashes in GTA V
        int[] slipperyMaterials = { 
            -1286696947, // Ice
            -840216541,  // Snow
            1333033863,  // Wet concrete
            -1885547121  // Mud
        };
        
        foreach (int mat in slipperyMaterials)
            if (materialHash == mat) return true;
        
        return false;
    }
    
    private void ShowNotification(string msg)
    {
        Function.Call(Hash._SET_NOTIFICATION_TEXT_ENTRY, "STRING");
        Function.Call(Hash._ADD_TEXT_COMPONENT_STRING, msg);
        notificationHandle = Function.Call<int>(Hash._DRAW_NOTIFICATION, false, true);
    }
}

/* 
 * INSTALLATION INSTRUCTIONS:
 * 
 * 1. Install ScriptHookV and ScriptHookVDotNet
 * 2. Compile this script to a .dll
 * 3. Place in GTA V/scripts folder
 * 4. Launch game
 * 
 * CUSTOMIZATION:
 * To change keybinds, modify these lines:
 * - toggleKey = Keys.F8;        // Toggle TC on/off
 * - increaseKey = Keys.OemCloseBrackets;  // Increase TC strength (])
 * - decreaseKey = Keys.OemOpenBrackets;   // Decrease TC strength ([)
 * 
 * Available Keys: https://docs.microsoft.com/en-us/dotnet/api/system.windows.forms.keys
 * 
 * PERFORMANCE NOTES:
 * - Sports cars are processed every frame for maximum responsiveness
 * - Regular cars skip frames to reduce overhead
 * - Non-car vehicles are ignored completely
 */