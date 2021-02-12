# SekiroControllerFix

== WHAT THE HELL IS THIS? ===

A program that injects into the Sekiro executable to fix controller behaviour, particularly when using DS4Windows and other peripherals connected to your PC.

=== WILL I GET BANNED? ===

I can't reasonably answer that with a definitive "no", so I won't take any responsibility for this triggering any kind of anti-cheat countermeasures, but Sekiro allows other programs such as Steam to perform similar injections onto the input layer, so it should be safe as far as I can determine.

=== HOW DOES IT WORK? ===

1. It hooks into DirectInput8 and prevents reporting any devices that do not contain "360" in the product name (which is how DS4Windows usually reports DualShock4/DualSense controllers). This is to fix an issue where having something like Fanatec Pedals or Wheel will for some reason cause the game to not listen to the actual controller you want to play with. I'd hoped to make this configurable so you could select a process yourself, but it's a bit of a pain and looking for "360" should solve the majority of cases.

2. It hooks into XInput and only reports one controller at the first user index, but it will assign whichever controller first receives a button input to that user index. This is to fix an issue where sometimes the only controller that's plugged in will be reported as the second controller, which can confuse Sekiro about which controllers are active.

=== HOW DO I RUN IT? ===

Just make sure SekiroInjector.exe and SekiroControllerFix.dll are in one folder and run SekiroInjector.exe prior to launching the game. It'll auto-hook into the game process when it starts and it should work fine (or at least better in some scenarios).

=== CREDITS ===

Detours library for hooking Win32 API calls for XInput.
