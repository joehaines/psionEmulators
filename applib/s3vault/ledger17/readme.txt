




                      Release notes for Ledger revision 17 

     
     Put LEDGER17.OPA  in an  \APP  directory and  install  it on  the  system
     screen.
     Make a directory called \APP\LED and put all the .OPO files in it.
     Make a directory called \LED.
     Installation is then complete.
     
     To create a new account, move on  the Ledger icon and choose New  File...
     (Psion-N). You can then type in the name of a new account, eg NATWEST for
     a bank account or FOOD for an expense account.
     
     A dialog will then appear asking for a long description of the account, a
     category (just type NONE for now) and a style - select Normal.
     
     In order to do anything useful with Ledger, it is necessary to create two
     accounts.
     
     Everything in ledger is done as transactions between two accounts - being
     paid is a transfer of money from  your SALARY account to a bank  account,
     taking money out of  the bank is  a transfer from a  bank account to  the
     CASH account and buying food is a transfer from CASH account to the  FOOD
     account.
     
     You will need to create these accounts yourself.
     
     Press the diamond key to switch between the Ledger and the Accounts view.
     The Ledger view  shows transactions  in an account,  whilst the  Accounts
     view shows details of all accounts.
     
     The menus are as follows:
     Account
          Most options are  obvious. Acct Information  allows the  information
     entered when the account  was created to  be modified. Style  Information
     may bring up information about the account specific to the present style.
     Transaction
          Only appears in Ledger mode. Again mostly obvious. Clear Transaction
     marks a transaction with a * and in some styles has additional meaning.
     Sort
          Obvious.
     Standing Orders
          Obvious.
     Special
          Update totals causes additional  entries to be  made to the  Account
     screen showing totals for  each category (as  entered in the  Information
     dialog).
          Integrity check checks the integrity of the account files and brings
     up warning dialogs if there is a problem. This rarely happens.
     
     Styles are my way of adding in additional functionality to Ledger without
     modifying the main .OPA file. They are .OPO files kept in \APP\LED.
     This test release contains a number of styles:
     BANK - This is for bank accounts. It provides three totals:
     Actual - the balance when all pending transactions have cleared.
     W/case - the "Worst Case Scenario" .  This is what your balance would  be
     if all outgoing transactions clear, but no uncleared income clears.
     Cleared - the cleared balance. What you bank says your balance is.
     Transactions are marked as cleared with the Transaction... Clear option



     
     BEVVY
     Beverage tracker.  This keeps  track  of how  much  beer is  drunk.  When
     transferring money to a beer account,  a dialog box is brought up  asking
     how much has been drunk.
     Summary information is given at the bottom of the ledger screen.
     
     INOUT
     Provides a plain total, a sum of incomes and a sum of expenditures.  This
     might be useful on  a credit card so  that you can see  how much you  are
     using the card.
     
     NORMAL
     Provides a single simple normal total.
     
     SUBACCT
     When transactions are made to and from a SUBACCT style account, a further
     dialog is brought up asking for a sub-account.
     Totals for each sub-account  account can then  be viewed with  Account...
     Style Information.
     I use  this for  keeping track  of loans  to other  people -  I have  one
     account for loans. Each person has their own subaccount within this loans
     account.
     This helps keep the Account screen uncluttered.
     
     Thanks
     Ben Clifford
     benc@dass.prestel.co.uk
     
