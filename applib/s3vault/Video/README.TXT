This program is a complete video database system. I've included a
sample database so that you can experiment with it. As it has been
released as shareware you are limited to using it as a viewer only.
It can cope with upto 8000 tapes, and uses compression to keep the
storage requirements as small as possible. It compressed a video
list of 620K to 171K.

The files included are

PC only files

Video.Txt       - Sample data file.
Videos.Bas      - Conversion program. In quick basic.
Video.Bat       - Batch file to create files and copy them to the Psion.
Install.Bat     - Install files on Psion.
Install2.Bat    - Rcom batch file to install onto Psion.
Cpyvideo.Bat    - Rcom batch file to copy files to Psion.

Psion files

                        {Binsort.Vid - Used for titles.
                       {Desc.Vid     - Compressed episode details.
Created by Videos.Bas {Ptr.Vid       - Details for each program.
                       {Tapes.Vid    - Informations on tapes.
                        {Titles.Vid  - List of titles.

Video.Rsc       - Help File.
Video.Vid       - Preferences and Status.
Video.Opa       - Application.

Format of Video.txt

* or ' ' if it has a * in front it is missing or I haven't got it yet
         eg for series I'm collecting at the moment.
Name     This is the name of the series or film. 
(n)      n indicates series #.
[n]      n indicates series # this is used for a new series for eg
         (1-5) would be used for original Twilight Zones and [1-3] 
         for the new colour series that came after.
n)       n indicates the episode #. 0 indicates unknown.
-        Some series are not broken down into series and
         episode format, so a dash is used.
Ep. Name The name of the episode if relevant.
*        If a * is present in the name it means I am missing part of
         the programme and want a better copy. 
nnnn     Tape number, I've used numbers over 3000 to indicate original
         tapes. You can change this number in the registered program.
Format   I've used codes here to give further info.
A        Animation.
F        Foreign subtitles on screen.
L        Long play.
M        Monochrome.
N        Nicam (stereo).
S        Super Vhs tape. (or beta if you use those)
T        subTitled, subtitled in English, foreign language dialogue.
W        Widescreen.
*        Missing a part of the programme.



