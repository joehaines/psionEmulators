===============================================================================
 [software name          ]  Samurai - My Success Story -
 [registration name      ]  haran
 [copyright person       ]  Cosmic Co., Ltd.
 [operation environment  ]  PSION Series5
 [development environment]  EPOC SDK Relese5
 [developer tool         ]  Microsoft Visual C++ Ver5.0
 [operation confirmation ]  PSION Series5
 [software type          ]  Freeware
 [e-mail                 ]@download@x-visions.com
 [web site               ]  http://www.x-visions.com
===============================================================================

 [Outline of application]
    - Samurai - My Success Story - is a simulation game in which a business-man
      climbs up the ladder of hierarchical Japanese company.
      The game operates on series 5 of the PSION.



 [Outline of the game]
    - A player climbs four official position:
      clerk -> section manager -> director -> president.


    - The game is constructed with four main stages (for each posistion) and
      three promotion examination stage.
      The four main stage games are: the memory, the paper-scissor-rock,
      the mole hitting, and the patting golf.

    - The flow of the game is as follows.
      *--------------------------*
      |      Game beginning      |
      *--------------------------*
                   |            
      *--------------------------*
      |The memory                |<-----[1]
      |(Clerk's main stage)      |
      *--------------------------*
                   |            
                   |Clear   
                   |            
      *--------------------------*
      |Section manager promotion |----->go back to [1] :No promotion
      |       examination stage  |----->proceed to [3] :Two ranks promotion
      *--------------------------*
                   |            
                   |Promotion   
                   |            
      *--------------------------*
      |The paper-scissor-rock    |<-----[2]
      |(Section manager          |
      |             main stage)  |
      *--------------------------*
                   |            
                   |Clear   
                   |            
      *--------------------------*
      |Director promotion        |----->go back to [1] :Demotion
      |       examination stage  |----->go back to [2] :No Promotion
      |                          |----->proceed to [4] :Two ranks promotion
      *--------------------------*
                   |            
                   |Promotion   
                   |            
      *--------------------------*
      |Mole hitting              |<-----[3]
      |(director main stage)     |
      *--------------------------*
                   |            
                   |Clear   
                   |            
      *--------------------------*
      |President promotion       |----->go back to [1] :Demotion
      |       examination stage  |----->proceed to [3] :No Promotion
      *--------------------------*
                   |            
                   |Promotion   
                   |            
      *--------------------------*
      |Patting golf stage        |<-----[4]
      |(President's amusement    |
      |                  stage)  |
      *--------------------------*
                   |            
      *--------------------------*
      |    All stages clear      |
      *--------------------------*


    - Promotion (Example: the clerk) 
        A player will advance to the section manager promotion examination stage
        (jump and item acquisition stage) by clearing the memory stage.
        When 100-190 points are acquired in the section manager promotion stage,
        the player is promoted to the section manager. Moreover, when 200 points
        or more are acquired, special promotion by two ranks is awarded.

    - However, dishonorable "Demotion" exists (as in the real life).

      ->Demotion 
        When the game result is remarkably bad in the each promotion examination
        stage (less than 50 points), the player is subject to "Demotion".
        (Return to the previous rank: ex. to the clerk, if the present official
         position is a section manager) 

      ->No Promotion
        When the score is less than 100 points in the each promotion examination
        stage, the player is subject to "No Promotion".  
        (Return to a main stage of the present official position) 



 [Install]
    Please install by either of the following methods.

    [Installation from PC to PSION Series5]
     Double click HARAN.SIS from PC and install according to the installer.

    [HARAN.SIS is copied from PC to PSION Series5 and installs.]
      1) start "Add/remove" from "Control panel" of PSION Series5
      2) click "Add new"
      3) select HARAN.SIS
      4) click "OK"
      Or, start HARAN.SIS directly.
      Please specify where to install after checking the empty RAM(1MB) 
      size of the machine.



 [UnInstall]
      1) start "Add/remove" from "Control panel" of PSION Series5
      2) select "HARAN1.00"
      3) click "Remove"



 [How to play]

    [Start]
    - Start the application by clicking "Haran" from - "Extras".


    [Operation]
    - Menu bar
        [File menu]
        - Start:Select either the Story mode or the Auto mode.
              Story mode -> a manual game mode.
                            It is not possible to select this while playing the
                            Story mode.
              Auto mode  -> an auto game mode.
                            It is not possible to select this while playing the
                            Auto mode.
        - Reset:returns to the start screen.
        - Exit :End the application.

        [Option menu]
        - Level:select the level of the game from Easy and Hard.
                Default is Hard.
                It is not possible to change in the game stage.  
        - Sound:ON/OFF switch. Default is ON.


    [Stages]
    - The Memory (Clerk's main stage) 
      ->It is a game to turn over the panel of 3*3 in the limited time, and
        to acquire three kinds of indispensable items as a clerk.

        Clear condition  :When all three items are collected in the limited
                          time, the stage is cleared.
                          However, if failed to collect all the three, the 
                          player's life is reduced by one and the same stage
                          restarts. 

        Operation method :Click "1-9" panels or is press the number key to click
                          the panels.
        Score            :When matched correctly, the "Basic point"
                          (The screen display:"Point" of the first high right
                          information panel) is added to the "Score".
                          When unmatched or "X" are pulled,  "Basic point"
                          is reduced by ten points. ("Basic point" won't be
                          a minus number.)
                          The remaining time is added to the "Score" when all
                          three kinds of items is acquired (ten points for each
                          unit of ramaining time), and the "Score" is added to
                          the "Total property".


    - Section manager promotion examination stage 
      ->This is a game to have a character jump in order to acquire items which
        is flowing in the upper part on the screen within the limited time.

        Clear condition  :When the norm (point) is achieved within the limited
                          time, the stage is cleared.  
                          However, if time is over and the norm has not been
                          achieved, the player is subject to "no promotion" or
                          "demotion" according to the score.
        Operation method :Use the "ENTER" button on the right of the character
                          or clicked or the "Enter" key on the keyboard to have
                          the character jump.
                          The height of the jump depends on the height of the 
                          power indicator.
                          (the power indicator is displayed on the screen only
                          when "Hard" level is selected in the Option menu.) 
        Score            :The score of each item acquired is added.
                          A promotion is awarded when the score is 100 points or
                          more, and when it is 200 points or more, two ranks
                          special promotion is awarded.  However, when the score
                          is less than 100 points , no promotion is given,
                          and the screen returns to the Memory stage.
                          When a promotion is awarded, the "Point" is 
                          reflected in the "Total property".
                          When two ranks special promotion is awarded, the bonus
                          point of 300 is added to the "Point", and the "Point"
                          is reflected in the "Total property".  
                          Wnen no promotion is awarded, the "Point" won't be 
                          reflected in "Total property".  
        Items            :Business card = 10 points
                          Portable telephone = 20 points
                          Note personal computer = 30 points
                          ?mark = +50 points or -50 points or +1 Life


    - The Paper-Scissor-Rock (Section cmanager's main stage) 
      ->It is a game to play the P-S-R and try to win three times within the
        limited time.

        Clear condition  :Win three times within the limited time, the  stage is
                          cleared.  If failed to win three times, the player's 
                          life is reduced by one and the stage restarts.
        Operation method :Click either "rock" or "scissor" or "paper"
                          line on the right side of the screen.
        Score            :100 points for one victory, 200 points for two 
                          successive victories and 300 points for three
                          successive victories are given.
                          The remaining time is added to the "Score" (ten points
                          for each unit of time), and the "Score" is reflected 
                          in "Total property".


    - Director promotion examination stage 
      ->It is a game to have the character jump in order to acquire items which
        is flowing in the upper part on the screen within the limited time.
        The item movement speed is quicker than that of in the section manager
        promotion stage.

        Clear condition  :When the norm (point) is achieved within the limited
                          time, the stage is cleared.  
                          However, if time is over and the norm has not been
                          achieved, the player is subject to "no promotion" or
                          "demotion" according to the score.
        Operation method :Use the "ENTER" button on the right of the character
                          or clicked or the "Enter" key on the keyboard to
                          have the character jump.
                          The height of the jump depends on the height of the 
                          power indicator.
                          (the power indicator is displayed on the screen only
                          when "Hard" level is selected in the Option menu.) 
        Score            :The score of each item acquired is added.
                          A promotion is awarded when the score is 100 points or
                          more, and when it is 200 points or more, two ranks
                          special promotion is awarded.  However, when the score
                          is less than 100 points , no promotion is given, and
                          the screen returns to the Paper-Scissor-Rock stage.
                          When a promotion is awarded, the "Point" is 
                          reflected in the "Total property".
                          When two ranks special promotion is awarded, the bonus
                          point of 500 is added to the "Point", and the "Point"
                          is reflected in the "Total property".  
                          Wnen no promotion is awarded, the "Point" won't be 
                          reflected in "Total property".  
        Item             :Beer = 10 points
                          Microphone = 20 points
                          Document = 30 points
                          ?mark = +50 points or -50 points or +1 Life


    - The Mole Hitting (Director's main stage)
      ->It is a game like the mole hitting.  Check the items shown on the upper
        part of the screen ("Next" column) and try to acquire matching items 
        displayed on the lower part of the screen.

        Clear condition  :When three kinds of items are acquired in the
                          limited time, the stage is cleared.
                          However, failed to collect all the three, the player's
                          life is reduced by one and the stage restarts.
        Operation method :Quickly click the matching items on the screen as it
                          is shown on the "Next" column.
        Score            :When acquired one item, the "Basic point" (displayed 
                          as "Point"on the top of the information panel on the 
                          right)is added to the "Score".

                          When a wrong item is acquired, the point is reduced by
                          10.  Tne "Basic Point" won't be a minus number.

                          The remaining time is added to "Score" when all the 
                          three items are acquired (ten point for each remaining
                          unit of time), and the "Score" is reflected into the
                          "Total property".

    - President promotion examination stage
      ->It is a game to have a character jump in order to acquire items which 
        are flowing on the upper part of the screen within the limited time.
        The items move right and left unlike the section manager promotion
        examination stage and the director promotion examination stage.

        Clear condition  :When the norm (point) is achieved within the limited
                          time, the stage is cleared.  
                          However, if time is over and the norm has not been
                          achieved, the player is subject to "no promotion" or
                          "demotion" according to the score.
        Operation method :Use the "ENTER" button on the right of the character
                          or clicked or the "Enter" key on the keyboard to
                          have the character jump.
                          The height of the jump depends on the height of the 
                          power indicator.
                          (the power indicator is displayed on the screen only
                          when "Hard" level is selected in the Option menu.) 
        Score            :The score of each item acquired is added.
                          A promotion is awarded when the score is 100 points or
                          more.  However, when the score is 50-90, no promotion
                          is given, and the screen returns to the Mole Hitting
                           stage. And when the score is below 50, the player is 
                          demoted to the section manager, and the screen 
                          returns to the Paper-Scissor-Rock stage.
                          When a promotion is awarded, the "Point" is 
                          reflected in the "Total property".
                          When no promotion is awarded, the "Point" won't be 
                          reflected in "Total property". 
        Item             :Sake holder = 10 points
                          Fan = 20 points
                          Golf club = 30 points
                          ?mark = +50 points or -50 points or +1 Life


    - The Patting Golf (president's amusement stage) 
      ->It is the patting golf game.

        Clear condition  :If the ball is safely cupped-in, all stages are
                          cleared.
                          When failed, the player's life is reduced by one and 
                          the stage restarts.
        Operation method :Control direction by using "<-" button or "->" key of
                          the keyboard and clicked "ENTER" to hit the ball.
                          Stroke power depends on the height of the power 
                          indicator.
        Score            :When safely cupped-in, 1000 points are added to the  
                          "Score", and the "Score" is reflected in the
                          "Total property".

    [Game over]
       If missed when the life is 0, the game is over.  The player is fired,
       or his/her official position is now a president, he/she retires.

    [Your Market Value]
       "Your Market Value" will be calculated when a player retires.
       "Your Market Value" = Total property / years worked * 100 



 [About the age]
    The age can be up to 100 years old. There is no weakness by aging.



 [Releases]
    Oct.5,1999 Version 1.00
        First release!
