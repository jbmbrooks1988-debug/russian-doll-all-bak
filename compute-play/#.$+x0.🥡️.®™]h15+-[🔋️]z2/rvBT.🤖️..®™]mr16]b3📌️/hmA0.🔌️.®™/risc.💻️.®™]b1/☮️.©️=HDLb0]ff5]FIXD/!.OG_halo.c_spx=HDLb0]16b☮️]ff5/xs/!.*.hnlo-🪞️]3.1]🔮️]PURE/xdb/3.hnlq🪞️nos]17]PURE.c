//////////🦘️


#include <stdio.h>
#include   <stdlib.h>
#include   <string.h>
#include  <stdio.h>
#define NAND(a, b)  !(a & b)

#define BITESIZE 64

unsigned int cli_0[BITESIZE];
int tape_yr_0[BITESIZE];

int tape_count_0 = 0 ; // this isn't the same as clock
			//i think all chips run on clockcount.
// https://www.falstad.com/circuit/ (swap clock 4 switch^)<-halo aped this
			// follow halo , it literally worked for master-slave.
	// i believe this makes VNGL EZ. cuz clock is master'd -> can hook as A|B
int clk_count_0 = 0 ; 

char *orig_register_a = NULL ;
char *orig_register_b = NULL ;
char *orig_register_c = NULL ;
char *orig_register_d = NULL ;


//int int_temp_e ;

unsigned int int_reg_a ; 
unsigned int int_reg_b ; 
unsigned int int_reg_c ; 
unsigned int int_reg_d ; 

unsigned int int_temp_e ;
//👑️
//🤴🏽️
int prbg_random(){
}
//🤴🏽️

//👸🏻️
int magic_array[256];
int magic_distillation(){
}



int setup_vmem(){
 printf("setup_vmem:\n");
 int i;
    for (i = 0; i < BITESIZE; i++) {
       
      tape_yr_0[i] = -2 ; // 46 = "." (we dont need ascii yet i guess w/e)
      			//just print ascii codes 4 now w/e 
      			// can store as -2 or w/e do w/e u want.
      			// can store as 3 also w/e 
      cli_0[i] = -2 ; // ill just store 2 add + 48 ❤️‍🩹️
      
       printf("%c", tape_yr_0[i] + 48);
    }
    
    printf("\n");
}



int print_tape(){

printf("print_tape: \n");
 int i;
    for (i = 0; i < BITESIZE; i++) {
        printf("%c", tape_yr_0[i] + 48); // y 48? ascii
  
    }
    
   printf("\n");
   
     for (i = 0; i < BITESIZE ; i++) {
        printf("%d ", tape_yr_0[i]);
    }
    printf("\n");

}



int bin2dec_hvlq_4(){

 int_reg_a = dec_bin(orig_register_a);
    int_reg_b = dec_bin(orig_register_b);
    int_reg_c = dec_bin(orig_register_c);
    int_reg_d = dec_bin(orig_register_d);

printf("reg_a: %u\n", int_reg_a);
    printf("reg_b: %u\n", int_reg_b);
    printf("reg_c: %u\n", int_reg_c);
    printf("reg_d: %u\n", int_reg_d);

}



 
/// do this by storing them in an array 
int dec_bin(char *bin_str) {
    int decimal = 0;
    int power = 1;

    // Reverse the binary string
    char *rev_str = strdup(bin_str);
    for (int i = strlen(rev_str) - 1; i >= 0; --i) {
        if (rev_str[i] == '1') {
            decimal += power;
        }
        power *= 2;
    }

    free(rev_str);

    return decimal;
}


int dec2bin_hvlq_4(){}



int tx_sig_logic(){

// wut if i get it wrong!? w/e fix it


////////////////////////////////
 if (int_reg_a == 0 && int_reg_a == 0) {
        int_reg_a = 0;
    }
     if (int_reg_a == 0 && int_reg_a == 1 || int_reg_a == 1 && int_reg_a == 0) {
        int_reg_a = 0;
    }
    /*
     if (int_reg_a == 1 && int_reg_a == 0) {
        int_reg_a = 0;
    }
    */
     if (int_reg_a == 1 && int_reg_a == 1) {
        int_reg_a = 2;
    } 
    //////////////////////////////
    //  extra = 2 & 3 ... ( the hell ? 3 = empty 2 = high z... w/e ( 2 isn't used ever
    // its only confusing 2 someone who is "ignorant" its smol sig its "PHY RIGHT" w/e
    // use nunq if u dont like, cant help u with 3 tho ;) (0 is a hot sig) 3 = -1 in4bit) 
    
    ///////////////2
     if (int_reg_a == 0 && int_reg_a == 2 || int_reg_a == 2 && int_reg_a == 0) {
        int_reg_a = 1;
    }
     if (int_reg_a == 2 && int_reg_a == 1 || int_reg_a == 2 && int_reg_a == 1) {
        int_reg_a = 2;
    }
     if (int_reg_a == 2 && int_reg_a == 2) {
        int_reg_a = 0;
    }
  
    ///////////////2
    
    ///////////////3
      if (int_reg_a == 3 && int_reg_a == 3) {
        int_reg_a = 3;
    }
    
     if (int_reg_a == 0 && int_reg_a == 3 || int_reg_a == 3 && int_reg_a == 0) {
        int_reg_a = 1;
    }
    
     if (int_reg_a == 2 && int_reg_a == 3 || int_reg_a == 3 && int_reg_a == 2) {
        int_reg_a = 1;
    }
    
    ///////////////3
}

int clock_logic(){
// wut is this ? literally just tell me. can do rest later <3 
}

int switch_logic(){
// wut is this ? literally just tell me. can do rest later <3 
// is this even necessary? 
}


int modem_logic(){

// u have to open modem using "BINARY FILE NAME + .txt" str
///sprintf(system_target_1, "./+x/f2p.mod_2]rcv.+x %d", s_fd);
//orig_register_d

char modem_filename[69];

//sprintf(modem_filename, "./+x/f2p.mod_2]rcv.+x %d", s_fd);

printf("%s \n",modem_filename);

// i think the point is is modem (anything about 15  - then if its even its in , if its odd its out
// its actually saying in original halo that ONLY 16 is used... wut? no its jae , equal or over 16{}
//jae (jump if above or equal) 

}

int modem_out(){}

int modem_in(){}

int nunq_logic(){

/*
 // Set the values of int_reg_a (just to demonstrate)
    int_reg_a = 1;
    int_reg_b = 0; // Overwrite previous value with a new one


*/
   // Perform a logical NAND operation on int_reg_a, and store the resultin int_temp_e
   // int_temp_e = ~(int_reg_a & int_reg_a);
   //  int_temp_e = NAND(int_reg_a, int_reg_a);
   
    if (int_reg_a == 0 && int_reg_a == 0) {
        int_reg_a = 1;
    } else {
        int_reg_a = 0;
    }
int_reg_b = 3 ; // empty wire 

    // Sanity check: print the result
 //   printf("Result of NAND e : %u\n", int_temp_e);
    printf("Result of NAND a : %u\n", int_reg_a);
     printf("Result of NAND b : %u\n", int_reg_b);


}



int skip_to_logic(){




switch (int_reg_c) {

   // Handle the range 16 to 255 using an if statement
           
        case 3:
            printf("3:skip logic \n");
            
            break;
        case 8:
            printf("8 = tx:\n");
            tx_sig_logic();
            break;
        case 11:
            printf("11\n");
            break;
               case 16:
            printf("modem:\n");
            break;
               case 7:
            printf("clock log: \n");
            break;
                 case 6:
            printf("switch 2 log?\n");
            break;
                case 9:
            printf("nand:\n");
            nunq_logic();
            break;
           
        default:
            printf("Unknown default\n");
            break;
            
    }
 

}

int pre_wire(){
//
if(int_reg_a == 3){
 int_reg_a =  tape_yr_0[tape_count_0]  ;

if (int_reg_a == -2){
int_reg_a = 3;
}

}

if(int_reg_b == 3){
 int_reg_b = tape_yr_0[tape_count_0] ;


if (int_reg_a == -2){
int_reg_a = 3;
}
}


}

int post_to_wire(){
    printf("vibe check %d \n", int_reg_c);
     if (int_reg_c >= 16 && int_reg_c <= 255) {
                printf("16-255: store in wire\n");
            } else {
                printf("Value out of handled cases: %d\n", int_reg_c);
            }
            

}
int post_to_tape(){

// how does it handle 2 full a& b ? 
// if there is something in "d" store it 2 ram...
// i think 0-3 are reserved for safety w/e
/*
if(int_reg_d > 3){

tape_yr_0[int_reg_d] = int_reg_a;

int_reg_d = 3 ;
}
*/
//silenced feb3-25
//printf();
tape_yr_0[tape_count_0] = int_reg_a; //addded fb3-25

}

int post_to_cli(){

// only have to "fprintf once, cuz appending ⚠️

printf("clock count = %d \n",tape_count_0);
printf("cli_out:");
 int i;
    for (i = 0; i < tape_count_0 ; i++) {
        printf("%d ", tape_yr_0[i]);
    }
    printf("\n");


tape_count_0++; 

}
////////////

///🧿️🧿️🧿️🧿️🧿️🧿️🧿️🧿️🦘️
int halo_fx(){
printf("in halo fx");
// start logic here , main is cluttered and functional effoff

//steps
//3 = empty wire 
//////////////
// int vwire_array_512[512];
// int cli_mem_array_512[512];  // int can suffice and even print. 
// that may literally be all the storage vars...

// also scooting b wire to a wire, try to skip to logic 
/////////////

//pre_wire(); // pull from mem {w8ing...}
skip_to_logic();
//post_to_wire(); //📅️mar7 {w8ing}
post_to_tape(); // scoop to mem
print_tape(); // for debug w/e (tape can be 64 bits btw)
		
post_to_cli();
//📅️mar7🥱️
// this may seem boring. but i just fixed post2 tape]+EZ.AF] ie: its nearly done
// this isnt assembly , that tricked u...
// coming 2 this from "scratch" it maybe a "bit tricky" but ez enuff
// if u dont bake bugs in each step. w8 4 full b4 orb.
// this tape is already better than halo.s's btw. sorry🤷🏽‍♂️️
// tho im actually not sure. is 0 supposed 2 post  0 or blank...hi-z...
//(there are no empty spaces...
//
}












/////////////////////////////////

int test(){
 printf("4reg OG?\n");
    printf("reg_a : %s\n", orig_register_a);
  printf("reg_b : %s\n", orig_register_b);
    printf("reg_c : %s\n", orig_register_c);
  printf("reg_d : %s\n", orig_register_d);
}



////////////////////
int main(int argc, char **argv) {
    int i, j;
  
    
        FILE *fp1 = NULL;
    FILE *fp2 = NULL;
    FILE *fout = NULL;

    if (argc == 1) { // No arguments provided
        fp1 = fopen("f$+/halo_in.txt", "r");
        fp2 = fopen("f$+/halo_out.txt", "w");
        fout = fopen("f$+/halo_memst8.txt", "w");
    } else if (argc >= 3) {
        char *filename;
        for (i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "-1") == 0) {
                filename = argv[i + 1];
                fp1 = fopen(filename, "r");
                i++; // Skip the filename
            } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "-2") ==
0) {
                filename = argv[i + 1];
                fp2 = fopen(filename, "r");
                i++; // Skip the filename
            } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "-3") ==
0) {
                filename = argv[i + 1];
                fout = fopen(filename, "w+");
                i++; // Skip the filename
            }
        }
    }

    if ((fp1 == NULL) || (fp2 == NULL) || (fout == NULL)) {
        printf("Error opening files.\n");
        return 1;
    }
    ///
    
      printf("debug.\n");

   int count = 0;

    char c;
    while ((c = fgetc(fp1)) != EOF) {
        if (c == '0' || c == '1') {
            count++;
        } else if (c == '#') {
            while (c != '\n' && c != EOF) {
                c = fgetc(fp1);
            }
        }
    }

 //   fclose(fp1);

    rewind(fp1);
    printf("Count: %d\n", count);


    int count_again = 0;
    char halo_in[count];
    while ((c = fgetc(fp1)) != EOF) {
        if (c == '0' || c == '1') {
            count_again++;
            halo_in[count_again - 1] = c; // store the character in the array
        } else if (c == '#') {
            while (c != '\n' && c != EOF) {
                c = fgetc(fp1);
            }
        }
    }

    for (int i = 0; i < count_again; i++) {
        printf("%c", halo_in[i]);
    }
    printf("\n");

// char halo_in[count];


int instruction_size = 64 ;
setup_vmem(); //may7. moved this 2 get tape right. no clue wut it is...


char register_a[instruction_size];
char register_b[instruction_size];
char register_c[instruction_size];
char register_d[instruction_size];


orig_register_a = (char*) 
malloc(instruction_size * sizeof(char));

orig_register_b = (char*) 
malloc(instruction_size * sizeof(char));

orig_register_c = (char*) 
malloc(instruction_size * sizeof(char));

orig_register_d = (char*) 
malloc(instruction_size * sizeof(char));


    printf("INSTRUCTIONS \n\n");
   
for (i = 0; i < count; i += instruction_size * 4) {
    // Store 'instruction_size' characters from 'halo_in' into registers
    strncpy(register_d, &halo_in[i], instruction_size);
   register_d[instruction_size] = '\0'; // Null-terminate the string

  strcpy(orig_register_d,register_d);
    register_d[0] = '\0';   // Reset register_a for next iteration


 //  printf("reg_a : %s\n", orig_register_a);

    strncpy(register_c, &halo_in[i + instruction_size], instruction_size);
    register_c[instruction_size] = '\0'; // Null-terminate the string

 
     
  //orig_register_b = register_b;  // Store original value of register_a
  strcpy(orig_register_c,register_c);
    register_c[0] = '\0';   // Reset register_a for next iteration


 //  printf("reg_b : %s\n", orig_register_b);

    strncpy(register_b, &halo_in[i + 2 * instruction_size], instruction_size);
    register_b[instruction_size] = '\0'; // Null-terminate the string

 // printf("reg_c : %s\n", register_c);
  
  //orig_register_c = register_c;  // Store original value of register_a
    strcpy(orig_register_b,register_b);//
  
    register_b[0] = '\0';   // Reset register_a for next iteration


 //  printf("reg_c : %s\n", orig_register_c);

    strncpy(register_a, &halo_in[i + 3 * instruction_size], instruction_size);
    register_a[instruction_size] = '\0'; // Null-terminate the string

 
  //orig_register_d = register_d;  // Store original value of register_a
  
    strcpy(orig_register_a,register_a);
    register_a[0] = '\0';   // Reset register_a for next iteration


   //test();
   bin2dec_hvlq_4();
   
   halo_fx(); ////////////🧿️🧿️🧿️🧿️🧿️🧿️🧿️🧿️🧿️🧿️🧿️🦘️
  // exit(911);
}


    return 0;
}

//📮️🇬🇮️ ur better off doing on ur own. since u know wut ur doing.tx was valid8d tho🍀️💌️


/*

####################################reserved style (4 llms)
4 	04 	0000 0100 	&#4; 	EOT 	End of          = reserved
5 	05 	0000 0101 	&#5; 	ENQ 	Enquiry 	= switch_0 = "^"
6 	06 	0000 0110 	&#6; 	ACK 	Acknowledge     = switch_1 = "^^"
7 	07 	0000 0111 	&#7; 	BEL 	Bell            =clock oscillator ":" # NAND FX = "@"

8 	08 	0000 1000 	&#8; 	BS 	Backspace 	= reserved Transistor FX "|"						
9 	09 	0000 1001 	&#9; 	HT 	Horizontal Tab  = # quick nand fx
10 	0A 	0000 1010 	&#10; 	LF 	Line Feed       = reserved/pmos[ no , is bjt+-]
11 	0B 	0000 1011 	&#11; 	VT 	Vertical Tab    = reserved

12 	0C 	0000 1100 	&#12; 	FF 	Form Feed       = reserved
13 	0D 	0000 1101 	&#13; 	CR 	Carriage Return = reserved
14 	0E 	0000 1110 	&#14; 	SO 	Shift Out	= modem in 
15 	0F 	0000 1111 	&#15; 	SI 	Shift In	= modem out  (grnd? ) 

the rest (16-31) can be used for local memory , 
before we offload into external "MS flip flop states"

*/




//🤫️
// in future u may choose 2 convert 2 hex 2  bin 
// or dec 2 bin *(entire program 
// or even  8bit 2 64 token wise
// OR EVEN "SHVS/C/JS 2 BIN.hvlq . w/e 
// thats not for this , we assume it all comes in as 64bit bin
//>b-but ur converting 2 dec and bac again! < shhhh
//🤫️

