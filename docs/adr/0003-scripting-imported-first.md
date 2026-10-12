# Scripting Module Imported Before All Other Modules

`EngineContext` imports the scripting backend (AngelScript) as its first module, before every other engine module. Other modules register their components and types for scripting from inside their own constructors, which requires the scripting backend to already exist. Importing scripting in any other position causes those registration calls to run against an uninitialised backend and silently produce incomplete script bindings.
