# External localization overrides

DK Color Picker includes Korean and English strings inside the EXE, so these files are optional.

To override built-in text:

1. Create a `locales` folder next to `DKColorPicker.exe`.
2. Copy one of the `*.ini.example` files as `ko.ini` or `en.ini`.
3. Add only the keys you want to replace under the `[Strings]` section.
4. Restart DK Color Picker or switch the language once from the tray menu.

Unknown or omitted keys continue to use the strings built into the EXE.
