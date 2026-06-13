Name........: DLControls (OPL ListBox Module)
Platform....: PSION Series 5
Version.....: 0.92 (Beta)
Description.: A scrollable ListBox control module.
Author......: Damien Lewis

Features....: Allows pen and keyboard events.
              Number of items limited only by memory.
              
Functions...: ListBoxLink:(Function$)
                - Defines GLOBAL variables and sets default values.
              ListBoxCreate:(VisibleRows$, PixelWidth%, Font&, MaxStringWidth$, Style%)
                - Creates the ListBox. It is not visible until a ListBoxVisible is issued.
              ListBoxAddRow:(Text$)
                - Adds a row to the ListBox.
              ListBoxInsertRow:(Text$, Position%)
                - Inserts a row into the ListBox at the specified position.
              ListBoxDeleteRow:(Position%)
                - Deletes the row at the specified position.
              ListBoxVisible:(Xco-ord%, YCo-ord%, Status%)
                - Sets the coords where the ListBox will be shown, and whether it is
                  visible or not.
              ListBoxPointerEvent:(Window%, PenX%, PenY%, PenEvent%, Type&)
                - Sends pen or pointer events to the ListBox.
              ListBoxKeyEvent:(Event&)
                - Sends action events to the ListBox that are typically triggered by
                  using the keyboard.
              ListBoxFind:(Text$)
                - Searches all rows in the ListBox for a match or partial match based on
                  Text$. The search starts at the first byte of each row and is case
                  insensitive.
              ListBoxClose:
                - Destroys the ListBox, and frees associated memory.

Notes.......: To test this module, create a directory and copy the files contained in
              this .zip into that directory. Load and run the DLTest.opl program. This
              program shows examples of how to use the ListBox functions.

              This module was written in my spare time to address a need I had. I have
              released it for those who may also require this type of control. I will, at 
              some stage document this and add a few more bits and pieces. If you have
              any questions or ideas, send me an email. Enjoy.
              
Legal Stuff.: This program may be freely used and distributed. If you wish to use DLControls
              in a package that you write you are free to do so, provided that you acknowledge
              the author of DLControls in an apropriate place (the About box, or readme file).
              These programs come with no warranties or guarentees, implied or otherwise.
              The author is not responsible for any damage or loss due to using these programs
              or part thereof. Use this software at your own risk.
