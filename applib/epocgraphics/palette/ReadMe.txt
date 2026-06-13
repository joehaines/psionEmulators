Palette: Use all the colours on your Series 7 / netBook / netPad...!


What is Palette?
================
The EPOC ER5 Operating System on these machines supports 256 colours. However, the built-in graphics app, Sketch, only offers a very modest 16 colours. By modern PDA standards, 8-bit (256 colour) colour depth is pretty humble, but compared to just sixteen colours, it opens up the potential for a great deal more subtlety in colouring images.

Sadly, Sketch's apparently very limited capability in this regard, restricts the quality of the colour graphics that can be produced with it to the sophistication of a rudimentary child's crayon set! The irony is that it could easily have been made capable of much more when the original built-in ER5 Psion apps were rewritten for the new colour machines. Palette was conceived & developed to compensate for this deficiency, as far as possible.

First of all, let us state what Palette is not! It isn't a standalone 256-colour graphics application or, strictly, a plug-in for Sketch which upgrades it into a 256-colour version. It is a separate app. but, once launched, this utility integrates so seamlessly with Sketch (we hope!) that it gives the appearance of being part of it. This is essential in making the 'hidden' colour capability of Sketch not just accessible, but usable for practical purposes, i.e. without requiring excessive user effort/time with stylus or keyboard.



What is Sketch?
===============
Within the graphics limitations of a Psion, Sketch is actually a highly capable drawing and image manipulation program and already has the ability to handle the full ER5 colour palette. It is directly accessing & using the extra 240 colours which has not been implemented! In using Sketch to generate its own drawings, this would never become apparent to the casual user - it seems to be a little known fact that Sketch can manipulate ER5's 256-colour palette. This possibility first presented itself when importing a 256-colour MBM file into Sketch. All the extra colours are displayed by Sketch and are handled by its image editing functions in just the same way as the 16 'built-in' colours. The difficulty lies in generating these colours from scratch, within Sketch.



How does Palette work?
======================
Critically, as well as being able to import 256-colour MBM images into Sketch, its clipboard can handle the full ER5 colour palette, in exactly the same way as any of its 'natural' sixteen colours. This was demonstrated by the earliest testing of the concept behind Palette; a 256-colour palette was generated on a PC and converted into a read-only Sketch image. Selecting an area within one of its colour swatches, copying it, and then opening another Sketch file and keying Ctrl+V, successfully pasted the same non-native colour, thus enabling any of EPOC's 256 colours to be introduced into a working Sketch drawing. In this simplest form, using such an extended palette (EP), 'manually', enables Sketch to be used to generate 256-colour graphics.

The advantage of Palette is that it replaces the use of this separate Sketch file and develops it into a more integrated, efficient & powerful tool. It condenses the full EPOC palette into a window that can be easily brought up over a working Sketch and provides a user-friendly interface and quicker means of copy & pasting desired EP colours into the underlying drawing. The fundamental 'trick' to doing this is 'reverse engineering' the structure of Sketch's clipboard and manipulating it directly from Palette.



Quick Start Guide:
==================
If you read nothing else, read this! Most users (including myself!) tend to just install a new app, and start to 'play' with it straight away; they only refer to the Help file if & when they experience a difficulty in using some feature! Hopefully, Palette is user friendly enough to use 'straight out of the box' and its slightly more subtle & advanced features should become self-explanatory by just trying them out. However, there are some features of Sketch which are not widely known (and not all explicitly documented in Psion's own onboard Help) and which you may not be familiar with. They are not only very useful features for use within Sketch, generally; they are also crucial to making the most efficient & productive use of Palette's ability to enhance the power of Sketch. These techniques are described in detail, elsewhere in this Help file, but the essential features are outlined below.

To start using Palette, just first open a file in Sketch and then launch Palette from the Extras bar (the default preference settings will suffice, initially) - a window will appear over your working Sketch, showing all the colours now available to you in Sketch, using Palette. Simply tap on the desired colour to select it; tap on 'Copy' to send that colour to Sketch's clipboard; tap on 'Min' to minimise the Palette window and key Ctrl+V to paste a 'swatch' of the selected colour onto the underlying Sketch. Just as with any other pasting operation, the pasted pixel(s) will be still be 'floating' at this point and may be dragged around the screen (or moved using the cursor keys), until in the required position; tapping outside the swatch (or keying 'Enter') will 'fix' the swatch in its current location. As with any other drawing, editing, image manipulation or clipboard operation within Sketch, any Palette generated actions can be undone, using Ctrl+Z (5 undo/redo levels are available).

Note, that before it is fixed, a floating swatch can be resized by dragging on one of the 'handles' on its perimeter, either linearly using a side (edge) handle or proportionally using a corner one. Moreover, once fixed, any same-colour block can be extended, homogeneously, by first defining it with the Select Area tool and then extending the colour area in any single (linear) or dual (diagonal) direction. This is, in fact just a specific usage of Sketch's (proportional) resizing function. Given that Sketch cannot directly paint in anything but its basic sixteen colours, this is an essential tool for the realising of Palette's full utility and is also as near a facility as Sketch has to a 'flood-fill' tool.

If you have read this far(!), thank you! Hopefully, you will thank us, once you start using Palette. Now, please indulge us a little more and, at least, read 'Techniques, Tips & Tricks' as well!



Using Palette:
==============
Palette has been designed to be as simple & quick (and, hopefully, intuitive) as possible; not only as a virtue, in itself, but to make its use in parallel with Sketch as minimalistic as possible. This is important since, when colouring a complex or large image, it will probably be called up many times.

When launching Palette from the Extras Bar for the first time, the default settings will allow the new user to familiarise with how Palette integrates with and works with Sketch. Adjusting these settings (in particular, selecting AutoSelect & AutoPaste) will make subsequent use much quicker & easier. Once you are confident with Palette's principles & operation, try these options out.

Palette displays as a single window over a working Sketch. This user interface principally comprises the full 256-colour EPOC palette, in a 16 by 16 grid. It also features a preview window and four command buttons whose functions may be executed from the keyboard with the indicated shortcuts. All Palette's functions may also be accessed from a standard style cascading menu (key 'Menu' or tap on top left silkscreen icon), which additionally provides access to the About screen and this Help file.

Tapping on any of the 256 extended palette (EP) buttons will bring up that shade in the adjacent preview window (along with its RGB code in decimal or hexadecimal form). The extended palette may also be navigated with the cursor keys. Tapping on the 'Copy' button, keying 'Enter' or re-tapping the EP colour button (if AutoSelect preference is active) will copy that colour to Sketch's clipboard. Tapping on the Minimise/Hide button will then either minimise or hide Palette (according to the selected preference) bringing the underlying Sketch back to the foreground (alternatively, tapping anywhere outside Palette's window will always minimise it). Now keying Ctrl+V (or using the left-hand silkscreen Cut, Copy & Paste menu) will paste the selected colour into the top-left corner of the Sketch. The size of this swatch can be adjusted in the Preferences dialog box.

As with any other copy & paste operation, the pasted EP colour swatch remains floating until fixed with a tap anywhere outside it, or by keying 'Enter'. Prior to this, it may be dragged around the screen, and also resized, rotated, flipped, etc. (i.e. any applicable Sketch function). At any time, while still an active 'selected area', it may also be re-copied, if desired, with Ctrl+C; subsequent Ctrl+V's will then re-paste the swatch in its last modified form. A floating swatch may also be translated around the screen using the cursor keys; this Sketch feature incorporates two-speed auto-repeat.

If you are unhappy with a pasted & fixed EP swatch, simply key Ctrl+Z to undo it (Sketch supports five levels of undo & redo). If you really foul up, there's always the 'Revert to Saved' option!  ;¬)  ...having said that, although I have never experienced a crash within Sketch (and Palette has proved equally stable & benign in use with Sketch), do use Ctrl+S regularly to save work in progress, especially when working on a complex drawing.

After either fixing or undoing, keying Ctrl+V again, will repaste the same swatch into the top-left corner of the image (as currently visible on-screen), without needing to call up Palette again. It is only necessary to bring Palette back to the foreground when a different EP colour (or different initial pasted swatch size) is required. If hidden, Palette can be called up with its Hotkey or from the Extras Bar (or 'Open files/programs' dialog box); if it is minimised, just tap on its button in the top-right corner of the screen.

Activating both AutoSelect & AutoPaste from the Preferences dialog box, allows EP colours to be selected and pasted with half the number of pen or keyboard actions - just two taps on one of Palette's EP buttons will immediately paste a swatch of that colour into the underlying Sketch. This double tap will take considerably less than half the time required to execute four separate stylus/keyboard actions.

Give Palette a try and get colouring!  :¬))



Techniques, Tips & Tricks:
==========================
Apart from using AutoSelect & AutoPaste, the main shortcut to using Palette effectively & efficiently is just a little practise!

However, there are techniques which allow the user to save time & tedium in creating & manipulating images. These relate to Sketch itself, rather than Palette, but it is particularly important to understand & apply them, in order to be to take advantage of Palette's potential to enhance the power of Sketch, without incurring excessive time overheads.

The most valuable of these techniques is a specific use of Sketch's resize facility and approximates to a manual 'flood-fill' function. This is important because although, regrettably, Palette cannot enable Sketch to draw, using its paintbrush or aerosol effect tools, in any colours beyond its basic sixteen, it is ideally suited to colouring-in B&W or greyscale drawings and graphics.

First of all, ensure Sketch has its Select Area tool activated (top-left toolbar button or Shift+Ctrl+Z); then select any solid, single colour area by tapping on one point and dragging out from that corner to the desired quadrilateral; then, having released the pen, drag on any side of the quadrilateral to extend the EP colour block into any adjacent area of white or other dissimilarly coloured pixels. This is a very quick way of filling large, contiguous, yet irregular areas with the same EP colour.

Note, that the initially selected square or rectangular area can comprise any number of pixels greater than one, i.e. it can even be just a strip, one pixel wide. This can be selected by just tapping & dragging the stylus in a straight line horizontally or vertically. In this instance, while being selected, the 'area' will not be marked by the usual, finely delineated rectangle (with the perimeter line aligned along the centreline of those outermost pixels included in the area); instead it will be indicated by a single fine line, until the pen is released, when the usual 4 side & 4 corner handles will appear (located immediately outside the selected pixel area). Having tapped, dragged (along only one axis) & raised the pen, the strip of pixels may now be extended, linearly, by dragging on one of the end side (edge) handles. Thus, any area of EP colour greater than a single pixel may be extended without having to repeatedly paste, drag & fix.

The only difficulty encountered in using this method is in colouring 'around' single pixel corners, since one cannot extend from the single 'corner' pixel in the direction perpendicular to that from which it was approached. If the adjacent pixel beyond the corner is white (particularly if working in 'transparent' mode), one workaround is to select it with the required EP pixel and drag them at right angles. Otherwise, a single pixel of the same colour can be pasted in and the newly formed adjacent pair of same-colour pixels dragged out along their new, common axis. Easier to do than explain!

It can sometimes also be useful to copy & paste a few, or even individual pixels from one part of a drawing to another. The linear area selection technique works equally well with the copy & paste function. Again, a single pixel cannot be selected, but if a pixel can be selected with an adjacent white one, this amounts to the same thing when pasting, if working in transparent mode (toggle on/off with Shift+Ctrl+T). Of course, Palette can also be used to paste in single pixels.

Note, if you don't relish repeatedly dragging your stylus across the touchscreen, when moving EP swatches from the top-left corner of the screen, where they are initially pasted by default, it is possible to move floating pasted EP swatches around the drawing, before fixing, using the cursor keys, instead.

Similarly, although the execution of most of the Palette & Sketch operations have been described here, using stylus/touchscreen inputs, keyboard shortcuts & the cursor keys can also be used for many operations, if preferred (as can the silkscreen Cut, Copy & Paste menu).



Palette Preferences:
====================
These comprise six user-selectable options:

(i) 	AutoSelect:  Checking this option allows an EP colour button to be double-tapped to select and copy it to the clipboard. This saves tapping the 'Copy' button (or keying 'Enter').
With only this option enabled, tap once on an EP button to preview/select it; tap a second time to copy it to the clipboard; tap on the Hide/Minimise button to bring Sketch to the foreground (or tap anywhere outside the Palette window to minimise it); key Ctrl+V to paste in the floating swatch.

(ii)	AutoPaste:  This option automatically pastes a floating EP colour swatch into the underlying Sketch as soon as it is copied to the clipboard. This saves two stylus/keyboard operations (Hide/Minimise and Paste).
With this option enabled only, tap once on an EP button to preview/select it; tap on 'Copy' to send it to the clipboard, hide/minimise Palette and paste into Sketch.
With AutoSelect option enabled, as well, tap once on an EP button to preview/select it; and tap a second time to copy it to the clipboard, hide/minimise Palette and paste into Sketch, i.e. just two pen inputs at the same location - a double tap - required to select any one of Palette's 256 colours and paste it into a drawing.

(iii)	AutoMinimise:  This option selects whether Palette may be hidden or minimised from its toolbar button. With 'Min' selected, Palette will minimise to the top right corner of the screen from where it may be tapped to restore it; with 'Hide' selected, it will disappear to the background. It may be returned to the foreground by using the Hotkey, the Extras Bar icon or 'Open Files/Programs' dialog box.
The AutoMinimise option selected will also determine whether Palette is sent to background or minimised when AutoPasting is activated.
Whichever option is toggled on, tapping outside Palette's maximised window will always minimise it.

(iv)	RGB Codes:  Selects whether the Red-Green-Blue colour code for the previewed EP colour is displayed in Hex code or Decimal form. These are the values used by EPOC ER5 to define colours displayed on-screen.

(v)	Paste Size:  This provides the user with a choice of three swatch sizes to be pasted into the underlying Sketch by Palette - one, two or five pixels square.
Remember that floating swatches may be resized into larger or smaller quadrilateral swatches by dragging their sides/corners, prior to fixing.

(vi)	Hotkey:  Allows the user to change the key which, combined with 'Ctrl+Fn' returns Palette to the foreground, when hidden or minimised.



Future plans...
===============
These are just some of the ideas we've had for future enhancements.  But just because they are (or are not) listed here isn't necessarily any indication that they'll definitely happen - they're just ideas for now...!

• AutoDrag n'Drop
• Drag-able Palette window
• Minimise to user selectable toolbar colour button instead of top right corner, reselect obscured button by 'drag & snap to' alternative
• User selectable paste size option
• Selective colour-themed extended palettes, e.g. reds, blues, greens, with bigger buttons for colour comparison/selection



Acknowledgements:
=================
Undoubtedly, Palette would not exist were it not for the persistence of one man - Mr. Lewis Barton (LRBarton@orange.net).

If I remember correctly, he and I came to the realisation more or less at the same time that you could cut and paste any of the Psion's 256 available colours into Sketch.  However, it was Lewis who mooted the original idea for Palette and it was Lewis who encouraged, cajoled, guided, nagged, managed, etc. me and above all, alpha-tested the heck out've everything!  All I did was type in the code in the correct order (usually!).  So if you want to thank anyone for Palette, thank Lewis!  :¬)

I also need to pay some thanks to Kevin Millican.  It was his little WebCol application that gave me the idea for the layout of Palette - thanks Kevin.



Compatibility:
==============
Palette has been designed to work primarily on the Psion Series 7, netBook, and netPad.



Standard disclaimer:
====================
Whilst we have tested Palette reasonably carefully, we cannot and do not give - or imply - any sort of warranty with it.  Nor will we accept responsibility for any data lost or damage caused as a result of using Palette.  I.e. use at your own risk!


Updates:
========
Any updates will be via the Pscience5 web site at:  http://www.pscience5.net


Best regards,
Martin Guthrie, September 2003
(martin@pscience5.net)
