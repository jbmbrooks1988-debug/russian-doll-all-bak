#🎡️🎡️🎡️🎡️🎡️🎡️🎡️🎡️🎡️🎡️🎡️🎡️🎡️🎡️🎡️

#having problems its not even doing 16 / 32 
#it seems 2 stop counting @ 8bit i may have to rewrite bin in
#and get a handle on the situation :/ 
#makes me wonder if it would be better 2 do it in c/js
#cuz thers probably peices u could peice 2 gether. w/e 
#just finish it in asm ofc. im sure youll get 2 the bottom of it
#cuz addrs obviously can handle the full 64. 
#maybe just do it full 64 or we works for u its very weird tho
#➕️
#youll probably be able 2 debug / fix 64 easier once 32 is done cuz +infrastruct-kruft
#https://github.com/copy/v86/blob/master/src/memory.js (see how this handles 64+mem. wtf?(in pages of 32...)
#U shouldn' need that tho cuz we HAVE count up 2 64bits reg space....

#could end up adding "line #" (as auto-comments (2 right, as well as "HEX of BIn")
#just for readablility (reading OTHER PPLS HEX (in c/elf/pe etc) 

#➕️
#u can also make this a "VHDL 2 HALO" compiler later, u just have to interpet that a 
#"NEW HEADER WANTS TO CHANGE COMMANDS"(its an ez add on , dont sweat it )<3
#➕️
#==================
#it should calc first number - compare 2 second if lower add if higher subtract just add first
#then it should copy  "7x(0111)" adding +1 address each time (ignoring the "11's")

#if "count is less than "2" grab base/mult...

#it could even be 4 "as in "bits / "current base" if u dont trust w/e base is "current"

#after all it could be wrong (it could be skipped or something and i dont think u wanna 
#fool with that when THIS Is so easy AND 2 STD...

#===================

#🎡️🎡️🎡️🎡️🎡️🎡️🎡️🎡️🎡️🎡️🎡️🎡️🎡️🎡️🎡️
#🌞️19-20 (no sooner) (go 2 bed well past 4 
#🎓️
#🚧️
#📅️does log use "load buffer?"
#🔏️
#mod spex latest? 
#do all modem "DYNAMIXALLY" DEFINATELY CUZ 3+ networking WILL BE DONE SOON (even if its "slower")
#tho i dont think it will be , this stuff runs "OS's" but maybe.
#DO MODEM strinc FEED (NOT 1pg BIT BUT SINCE ITS DYNAMIC LENGTH GETS LONGER , keep reading inc.

#u may not actually have to "LOAD FILE 2 buffer ? " how was cli doing it? (probably tho...)
#🔏️

#☎️ MODS = go 2 > 0-3 (instead of 15s , so i can store "FX" IN FX related wires...) 

#☎️ add modem io from 2 files . 📥️📤️
#++++++++++
# < ~[]           
#零一二三
#linq(0),i(1),h(2),sn(3) 
#~?
#0.零 (rei)zero ,1.一. (ichi) one , 二. (ni). two , 三. (san). three · 
#0 – 영 (yeong),1 – 일 (il),2 – 이 (i),3 – 삼 (sam)

#change legend 2 have "TX as 0100" for "LI"  <EZ just do it
#swap code words in txt
#ship that , no more mods ever probably...
#may change num buttons 2 ^'s later...
#🦢️
#*swap with | = il asap pls. 
								#then nand (as in 0101(2) txs)
								#then 110 = 0switch 111 =1 switch
								#then modem in = 1000 
								#then modem out = 1001
								#(then clock = 1010
								#(do this b4 modems pls)
								#(or u can do modems at
								#(1110 1111 for now w/e )
								#y rock the boat? (time isn't 8)
								#🦢️
#🗻️
#💾️
#may add save / load state later 
#u can use special mark 2 diff ram from clock and start clock load etc
#can trigger from cli of "HALO" (6-7)
#can probably also easily designate filename by typing it 
#but who wants 2 do that...(no autocomplete ofc)
#💾️
#may also do "STOp clock somehow , 2 get "psudo kbd input" till we have IO.
#it will just throw an "interupt" 2 our clock 
#maybe triggered with a "_fx__" dunno. 
#🗻️
#👷🏻‍♀️️
#also... y the hll are u doing "MODEM" right NOW? (we just finished mux
#(other than maybe their bored of making wires? 
#its good 2 have other things 2 "CODE ON" in a sp sort of way
#u could stop now tho , or even work on "save_load" state
#ur also probably gonna make a "stop4kbd" "interupt" 
#till u actually get "IO" ...

#📟️
#and finally u may even designate file / files from cli...
#would put save/load b4 modem io becaseu thats actually priority ... 
#📟️
#👷🏻‍♀️️

#⚠️
#just make sure whenever u stop(For the day!) the old code still works ! 
#⚠️
#🚧️
#🎓️




#🧿️the bin laying dormant helped wont get lost
#🧿️it shouldn't be THAT hard do it later if stress finish school
#🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️
#when u are able to optionally parse 16mux8 and do io (2riscv/arm and x86self). this code will be BEEST
#also its transferable 2 other archs, forever. 
#take ur time , u will get there quicker than u think
#comign from tokngn, i can vouce for this , took much longer
#2 learn asm than 2 code the engine
#🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️
#🧼️2do
#📆️may9]0.take file in from cli/bash only>go fast
#>[]


.section .data
#🧵️🧵️🧵️🧵️🧵️🧵️🧵️🧵️🧵️🧵️🧵️
prompt_str1:
    .ascii ":1=!1,:2=!2,:3=z1:,4=z2,5=Loop,6=SV,7=LD 8=14 9=24 " #save load state 
str1_end:
    .set STR1_SIZE, str1_end-prompt_str1
prompt_str2:
    .ascii "Enter second number: "
str2_end:
    .set STR2_SIZE, str2_end-prompt_str2
    
    clock_out_8bit:
.ascii "::::::::"
#🧵️🧵️🧵️🧵️🧵️🧵️🧵️🧵️🧵️🧵️🧵️
#######CONSTANTS########
#system call numbers
    .equ SYS_OPEN, 2
    .equ SYS_WRITE, 1
    .equ SYS_READ, 0
    .equ SYS_CLOSE, 3
    .equ SYS_EXIT, 60
    
     .equ ST_FD_OUT, 56
     
       .equ O_RDONLY, 0
    .equ O_CREAT_WRONLY_TRUNC, 03101
#standard file descriptors
    .equ STDIN, 0
    .equ STDOUT, 1
    .equ STDERR, 2
        .equ END_OF_FILE, 0  #This is the return value
                         #of read which means we’ve
                         #hit the end of the file
    .equ NUMBER_ARGUMENTS, 2
    #______________________
     #⌨️#⌨️#⌨️#⌨️#⌨️#⌨️#⌨️#⌨️
ioperm_message:
    .ascii  "ioperm() failed, run as root or sudo\n"
    #⌨️#⌨️#⌨️#⌨️#⌨️#⌨️#⌨️#⌨️#⌨️#⌨️
    
 
ascii_16_test_0:
.ascii "xxxxXXXXyyyyYYYY"
    
ascii_8_prog_0:
.ascii "12345678"

      #🧹️TEMT🧹️
     ascii_8_test_0:        #ascii_8_prog_0:(mt)
     .ascii "xxxxxxxx"
     ascii_8_test_0_end:
.equ ascii_8_test_0_len, ascii_8_test_0_end - ascii_8_test_0
    #🧹️TEMT🧹️

#🪣️🪣️🪣️🪣️🪣️🪣️🪣️🪣️🪣️🪣️🪣️🪣️🪣️🪣️🪣️
#*******"REGISTARS"*******
#__________PROG_16.4x4
#🗡️🗡️🗡️🗡️🗡️🗡️🗡️🗡️🗡️🗡️🗡️🗡️🗡️🗡️🗡️🗡️🗡️🗡️
#theres no header, im not gonna delete it right now and break prog. but will swap 4 out l8r
ascii_4_head:   #header
.ascii "0000"


ascii_4_a:
.ascii "0000"

ascii_4_b:
.ascii "0000"

ascii_4_i:
.ascii "0000"

ascii_4_outcode:
.ascii "0000"



ascii_4_test_0:
.ascii "same"     #"tes0"

ascii_4_test_1:
.ascii "same"   #"tes1"
#__________PROG_16.4x4
####
#--------------=====output tail or full as log 2 prove u have
ouput_4_0:
.ascii "0000"

#🧼️#🧼️#🧼️#🧼️#🧼️#🧼️#🧼️#🧼️#🧼️#🧼️#🧼️#🧼️
ascii_8_a:
.ascii "........"

ascii_8_b:
.ascii "........"

ascii_8_c:
.ascii "........"

ascii_8_d:
.ascii "........"

#🧼️
ascii_0x_32_a:
.ascii "................................"

ascii_0x_32_b:
.ascii "................................"

ascii_0x_32_c:
.ascii "................................"

ascii_0x_32_d:
.ascii "................................"

ascii_0x_64_a:
.ascii "................................................................"

ascii_0x_64_b:
.ascii "................................................................"

ascii_0x_64_c:
.ascii "................................................................"

ascii_0x_64_d:
.ascii "................................................................"


ascii_0b_32_a:
.ascii "00000000000000000000000000000000"

ascii_0b_32_b:
.ascii "00000000000000000000000000000000"

ascii_0b_32_c:
.ascii "00000000000000000000000000000000"

ascii_0b_32_d:
.ascii "00000000000000000000000000000000"
#🧼️#🧼️#🧼️#🧼️#🧼️#🧼️#🧼️#🧼️#🧼️#🧼️#🧼️#🧼️#🧼️#🧼️
#🪣️🪣️🪣️🪣️🪣️🪣️🪣️🪣️🪣️🪣️🪣️🪣️🪣️🪣️🪣️
#⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️
#🤺️
token_prog_0:
.ascii "0000"
#🤺️
token_test_prog_0:
.ascii "01!A"  "alpha bet for output registers...?4 now?"
		#using o for now cuz only 1 out w/e tho
		#not the biggest issue
#🤺️
token_array_multi_chip_0:
.ascii "10&aa1&bab!c"   #should we use "o" or "abc" not sure
#🤺️

#######this is for the actuall binary in.
token_array_16_2_4_0:
.ascii "0000"
#🤺️


#🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️
     
modem_test:  
#.ascii "????????.mdm\0"  
.ascii "modem_io/????????.mdm\0"
#🐏️🐏️
ram_4_0:
.ascii "0000" 
###################################
ram_16_0:
.ascii "0000\0000\0000\0000" 
# "." ? = space 4 last period? 🐑️<try count 1rst
ram_32_0:
.ascii "...............................x"  
# "." ? 🐑️
ram_64_0:
.ascii "................................................................"
#.ascii #"0000000000000000000000000000000000000000000000000000000000000000"
#.ascii #"________________________________________________________________"

ram_128_0:
.ascii "................................................................................................................................"

ram_128_v_0:
.ascii "................................................................................................................................"

ram_256_0:
.ascii "................................................................................................................................................................................................................................................................"


ram_256_v_0:
.ascii "................................................................................................................................................................................................................................................................"

ram_256_cli_0:
.ascii "................................................................................................................................................................................................................................................................"

ram_512_v_0:
.ascii "................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................"

ram_1024_v_0:
.ascii "................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................"



ram_64_history_0:
.ascii "................................................................"



ram_64_v_0:
.ascii "................................................................"


ram_64_v_history_0:
.ascii "................................................................"

ram_64_t_0:
.ascii "................................................................"


#👹️ just assume 4 bits. prog end? worry bout it when u get there

#🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️🐏️
#⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️
#➕️
#🎛️🎛️🎛️🎛️🎛️PRE-EQ (these should just be ascii compares...
	.equ reg_a  , 1000 
	.equ reg_b  , 1000
  	.equ inst , 10
  	.equ out , 0
  	.equ car , 0
  	#🎛️🎛️🎛️🎛️🎛️
  	#🎛️EQ
   .equ lo , 0b0000  # 0b0000 ? YES the only way m8
   .equ hi , 0b0001 
   #!!!!!!!!!!!!!!!!!!
   .equ not , 0b0100  #0b0100 
   
   	      #32     
   .equ not, 0b100001 #=33'!' #trying trix 4 "2nots"
   			#i seen in some dissasemblys
			#hope it uses first for (calls 2 ;)
			
	#.equ not # = 241 w/0 - 48 ... why?
	#NO the symbol is #1 but i guess thats alt
	#however 8bits only goes up 2 "127" so... naw...
			
   #!!!!!!!!!!!!!!!!!!!!!!
   #@@@@@@@@@@@@@@@@@@@@@@
   .equ nand , 0b0110 	  		
   .equ nand , 0b1000000 #=64'@' 
   #@@@@@@@@@@@@@@@@@@@@@@
   #---------------
   	
   .equ jno , 0b0010  #jo(for4bit) #cmp? sub? or w/e we need
   
   .equ jno , 0b101000 #= 40'('
   
   .equ jne , 0b0011  #jb? add ? (SHIFT LEFT ? RIGHT?) (ele) 
 .equ jne , 0b101001
   ########------3limit end
   .equ shl , 0b1000
   .equ shr , 0b1001
   #🎛️🎛️🎛️🎛️🎛️🎛️🎛️🎛️🎛️🎛️🎛️🎛️🎛️🎛️🎛️🎛️🎛️🎛️🎛️🎛️🎛️🎛️
   #🧱️
 .equ out_kode , 0b1010  #=10 (og out kode ) 
.equ out_kode ,   0b1001111  #=79 'O'
.equ karry_kode , 0b1011
.equ karry_kode , 0b1001011  #=75 'K'
#🧱️
#should !& binary just be value of "ascii? (no not gonna fit) 15.bits 
#unless u do 127 but i rather have relevant 4 2 8bit conversion for HDL tbh fam 8bit is ez.(alredy don) 
# https://commons.wikimedia.org/wiki/File:ASCII-Table.svg

#ie before u push the "token" ask if its "same number as (decided token revalue)(
#🔌️🔌️🔌️🔌️🔌️🔌️🔌️🔌️🔌️🔌️🔌️🔌️🔌️🔌️🔌️🔌️🔌️🔌️🔌️🔌️🔌️(grab from macrospec when ur ready)


  #\\\\\\\\\\\\\\\\\\\\\\\\\\\  
newline_out:
.ascii "\n"

ascii_sing:
.ascii "."

sing_test:
.ascii "엘"

err_msg_0:
.ascii "'!봇,審查,||<<>>'\n"  #Please Review Process (||<<>>)  
     err_msg_0_end:
.equ err_msg_0_len, err_msg_0_end - err_msg_0

#\\\\\\\\\\\\\\\\\\\\\\\\\\\



#.section .text


#-----------------
#segfaults if there is more code than bss#just needed 2 widen .bss. 
#.bss = bytes. (i dont think theres a limit but probably dont wanna abuse
  #-------------------------
    .section .bss
    .equ BUFFER_SIZE, 1600000*10 #130023424 #1024*1024*1024 #1,600,000 
    .lcomm BUFFER_DATA, BUFFER_SIZE    
    .equ test_SIZE, 500
    .lcomm   test_DATA, test_SIZE
    
     .equ load_SIZE, 500
    .lcomm   load_DATA, load_SIZE
    
    #using * 2 widen buffer has worked fine for now THANK GOD
#i've even seen it able 2 go wider? but it does get weird at some point
#but we will rock with it till it REALLY BLOWS...
.lcomm      input1  2
.lcomm      input2  2
.lcomm      ans     1

    #--------------------------
.section .text #not having this caused SEGFAULT ! watch 4 🔭️
#➕️
# file name 
.equ ST_LOG_OUT_0 , -16
.equ ST_LOG_OUT_1, 72
.equ ST_LOG_OUT_2, 80 #log out 8 bit 1  2 other first
#➕️

#STACK POSITIONS
    .equ ST_SIZE_RESERVE, 16 #i dont see a use 4 this
    			#but will keep eyes peeled
    			#as i go thru 1 by 1...
    .equ ST_FD_IN, 48 # i made these pos, keep an eye on that.
    .equ ST_FD_OUT, 56
    #u must add 16 to all these u simply must
    .equ ST_ARGC, 16#0      #Number of arguments
    .equ ST_ARGV_0, 24 #8   #Name of program
    .equ ST_ARGV_1, 32 #16   #Input file name
    .equ ST_ARGV_2, 40   #Output file name
    
    #⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️
    #⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️⭐️
    
        #➕️
filename_in:
    .ascii "@64]🍞️.hlo\0"
    filename_out:
    .ascii "out.yr\0"
    logname_out:
    .ascii "logout]🖨️.txt\0"
    
     modem_filename_in:
     .ascii "modem_io/00010000.mdm\0" #change these 2 zero & count up
     				#just swap ascii position
     				
     				
      modem_filename_out:
     .ascii "modem_io/00010001.mdm\0"
     

     
      modem_filename_test:
      .ascii "01234567"
      
    # .ascii "********.mdm\0" #change these 2 zero & count up
     				#just swap ascii position
     
     
      save_state_filename_in:
     .ascii "state_load_in]💾️.hlo\0"
      load_state_filename_in:
     .ascii "state_save_out]💾️.hlo\0"
#➕️

#🔏️🔏️🔏️🔏️🔏️🔏️🔏️🔏️🔏️🔏️🔏️🔏️🔏️🔏️
#✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️
#✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️
#✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️✡️
#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️
.globl _start
_start:
mov  %rsp, %rbp
#########################
#call test_hlsbyte_0
#call exit
#############
call sys_in

call sys_out #designate outfile

call log_open

#🎖️
call load_buff #puts buff in BUFF_DATA nothign fancy 

#call modem_open


#call save_state_open
#call load_state_open
#🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️#🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️#🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️

		
call buffer_comment_0
call buffer_stripper_0
#📍️📍️📍️📍️📍️📍️📍️
#call buffer_to_ram_64   #64-2-64 for now? bigger or smaller should be ez

call print_buff        #🧨️ < when i move this it clobbers wire ? 
call ascii_newline
call a2b_1
#call kbd2ascii_io_0   #🧿️


call log_out #will do manually 
#🖨️🖨️🖨️🖨️🖨️🖨️🖨️🖨️🖨️🖨️🖨️



#call print_err_0
call ascii_newline

#🖨️🖨️🖨️🖨️🖨️🖨️🖨️🖨️🖨️🖨️🖨️


#🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️#🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️#🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️🔯️

################


call ascii_newline

call exit
#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️#🐲️
 
#🏁🏁🏁🏁🏁
exit: #Table C-1. Important Linux System Calls 

#%eax]	Name]	%ebx]	%ecx]	%edx]	Notes]
#1]	exit]	ret val	_	_	exits
#3]	read]	file]	buf *rt	buf siz	reads into buffer
#4]	rite]	file]	buf *rt	buf siz	write buffer into file
#5]	open]	file]	optn	perm	opens file returns file/err
#6]	close	file]			closes file descriptor 


mov $1, %rax

#mov $201, %rbx# this is the status number we will

int $0x80# this wakes up the ker
#🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁🏁

#🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️

#🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️
.equ ST_BUFFER_LEN_0, 16
.equ ST_BUFFER_0, 24
#🗓️





#🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️
 .equ a_sum , 40
 .equ b_sum , 48
 .equ e_sum , 56  #this breaks everything for some reason...(doesn't register?)
 
 .equ c_sum , 72 # does this order madder?
 .equ d_sum , 64
 
  .equ clock_edge , 80  #1,or,0s
  .equ clock_count , 88 #:+-
  
  .equ switch_state , 96  #1,or,0s
  .equ switch_state_2 , 104  #1,or,0s
  
  #☎️
  .equ modem_in_stack_pos , 112  #replace with "3' everytime so it defaults (fx loops m8)
  .equ modem_out_stack_pos , 120  #1,or,0s
  
   .equ modem_in_single , 128  #replace with "3' everytime so it defaults (fx loops m8)
  .equ modem_out_single , 136
  
    .equ modem_in_count , 144  #replace with "3' everytime so it defaults (fx loops m8)
  .equ modem_out_count , 152 #1,or,0s
  

  #☎️
  
  #💾️
      .equ save_state_pos , 144  #replace with "3' everytime so it defaults (fx loops m8)
  .equ load_state_pos , 152 #1,or,0s
  

 #💾️💾️💾️💾️💾️💾️💾️💾️💾️
 .equ rax_pos , 160 
 .equ rbx_pos , 168 
 .equ rcx_pos , 176 
 .equ rdx_pos , 184
 
 .equ rsi_pos , 192 
 .equ rdi_pos , 200 
 
 .equ r8_pos , 208 
 .equ r9_pos , 216 
 .equ r10_pos , 224 
 .equ r11_pos , 232 
 
 .equ r12_pos , 240
 .equ r13_pos , 248 
 .equ r14_pos , 256 
 .equ r15_pos , 264 
 #💾️💾️💾️💾️💾️💾️💾️💾️💾️
#☸️
  .equ command_a , 272 #
   .equ command_b , 280 #
   .equ command_c , 288 #
   .equ command_d , 296 #
   
   .equ base_result , 304 #
#☸️ 
  .equ debug_0_loopcountpure , 312 #1,or,0s
 
 
 
 ####################
   .equ test_pos_0_0 , 16
 .equ test_inc_1_1 , 24
 .equ test_sum_2_2 , 32
 #🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️🐝️
  .equ ascii_ram_limit_low , 16
  .equ ascii_ram_limit_hi , 126
#########################################
#🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️


#🥼️🥼️🥼️🥼️🥼️🥼️🥼️#🥼️🥼️🥼️🥼️🥼️🥼️🥼️#🥼️🥼️🥼️🥼️🥼️🥼️🥼️
.globl a2b_1   #GODL = TOKENIZATION ⚔️⚔️⚔️⚔️
		#we will tokenize inst for now , then do full prog
		#⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️⚔️
a2b_1:
 pushq %rbp 
    movq  %rsp, %rbp
#_________________________
  start_test_a2b_1:  
 
 ################
 movq $0 ,clock_edge(%rbp) #tested=on
 movq $0 ,clock_count(%rbp) #tested=on
 ###########
  #movq $ram_256_v_0, %r12 #
   movq $ram_1024_v_0, %r12
  
  movq $48 , switch_state(%rbp)  
   movq $48 , switch_state_2(%rbp)  
   
   movq $'3' , modem_in_single(%rbp) #is reading out as "c'...
   movq $0 , modem_in_count(%rbp)
   movq   $1   , debug_0_loopcountpure(%rbp) 
  #🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️
   re_run_entire_with_new_clock_0:
 #🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️
 
 

  mov $BUFFER_DATA, %r8  #✅️do not change this! 

 mov $0,%r10   #❌️ counts up till abcd_ (then clear till next loop
 
 xor %r11  , %r11 
  movq $0 , %r11 # ram-stack count

 
  #📟️📟️📟️📟️📟️📟️📟️📟️📟️📟️📟️📟️📟️📟️📟️
   
   	movq $ram_256_cli_0, %r13
  #📟️📟️📟️📟️📟️📟️📟️📟️📟️📟️📟️📟️📟️📟️📟️
 mov $0,%r14  #✅️ do not change this is counting entire buffer
 #movq $ram_32_0, %r15#✅️ #is our "a2b STACK < DONT
 movq $ram_64_0, %r15
 #➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️💍️

 
# mov clock_count(%rbp) , %rbx
 #call exit
 #🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️
 
  
  #.equ clock_edge , 80  #1,or,0s
  #.equ clock_count , 88 #:+-
  
    ######################################
    #🦕️
    ######################################
     
#🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️<
 xor %rsi , %rsi 
    xor %rdi , %rdi 
    
  #🦕️ 
    movq  $0, %r14  #counting $BUFFER_DATA , loop,place
  #mov  $7, %rdi  #(counting 8 bit chunks)
  #mov  $31 , %rdi  #(counting 64 bit chunks)
  mov  $63 , %rdi 
  #🦕️
#🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️🛑️<
   start_reverse_ascii_in_only_1: 
   xor %rax , %rax 
    xor %rbx , %rbx 
     xor %rcx , %rcx 
    xor %rdx , %rdx 
   


   
   mov (%r8,%r14,1) ,%al 
    mov %al , (%r15,%rsi,1)  #🐞️bin is LSB 32 wise now (was MSB)
    #^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ < changed from "%rdx" got diff output
      #👁️‍🗨️️
 # mov %al , (%r12,%r14,1) 
     #👁️‍🗨️️
     
     	 dec %rdi #
    inc %r14 #is being used as buffer count  
    inc %rsi #is used 2 iterate 32
     
    
#u should beable 2 use the dec2bin formula but with add <!> sub
######################################☸️🤯️(its not binary yet
    cmp  $'0', %al #0 #low
  #  jl   end_a2b_1   # push_program_tokens_2 #next_byte
   jl clock_fx_start_0
    cmp  $'5' , %al #127 #his+
  #  jg   end_a2b_1   #  push_program_tokens_2 
  jg clock_fx_start_0
   
   #👾️ > premptively incing since # is already stored...
   #######################################
  
  
  #☑️  ⏺️
    #wanna compare first 64 and last of 1rst and second block.
  # cmp $2,  debug_0_loopcountpure(%rbp)
  # jne debug_0_loop
# cmp $1 , %rsi  
# cmp $64 , %rsi  
# jne debug_0_loop
   
 #  sub $48, %al
 #  mov %al , %bl
 #  call exit   #]these are correct, maybe its the next one...
   debug_0_loop:  
    #☑️   
    
    
cmp $-1 , %rdi
   jne start_reverse_ascii_in_only_1
   ########################
   #🀄️🀄️🀄️🀄️🀄️🀄️🀄️🀄️🀄️🀄️🀄️🀄️#red dragon
 	#mov $31 , %rdi   #reusing %rdi after %r15 (ascii) is full
 	 mov  $63 , %rdi 
 	mov  $0, %cl
 	#🀄️🀄️🀄️🀄️🀄️🀄️🀄️🀄️🀄️🀄️🀄️🀄️#
 	#can we just see %r15 ? 
 	  #👾️🐞️👾️🐞️👾️🐞️👾️🐞️👾️🐞️
 	 #♑️#♑️#♑️#♑️#♑️#♑️
      #♑️#♑️#♑️#♑️#♑️#♑️
    
    #♑️#♑️#
 #👾️🐞️👾️🐞️👾️🐞️👾️🐞️👾️🐞️
 	######################
 		 #➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️
 		 xor %rax , %rax 
 		mov %rax , test_sum_2_2(%rbp)
   #movq $0, test_sum_2_2(%rbp)

    #👾️ 
    
    
 	######################
 	 test_loop_a2b_1: #strip and load loop
#
#🔥️#🔥️#🔥️#🔥️#🔥️#🔥️#🔥️#🔥️#🔥️
xor %rax , %rax 
#if i didn't do this ALL HELL BROKE
#u should be fine for now,,, but just be sure 2 "clear out regs"
#u know? 
#🔥️#🔥️#🔥️#🔥️#🔥️#🔥️#🔥️#🔥️#🔥️
mov  (%r15,%rdi,1) , %al 
sub $48 , %al
shl  %cl , %rax 
#🔥️#🔥️#🔥️#🔥️#🔥️#🔥️#🔥️#🔥️#🔥️

add   %rax  , test_sum_2_2(%rbp) 

#debug_0_loopcountpure

#📋️
   dec %rdi
   inc %cl  

# cmp $8, %rdi
#cmp $32, %rdi
cmp $-1 , %rdi
 jne test_loop_a2b_1
 
  #👾️🐞️👾️🐞️👾️🐞️👾️🐞️👾️🐞️
  
  movq    test_sum_2_2(%rbp) , %rax 
#jmp dec_2_bin_0
 #👾️🐞️👾️🐞️👾️🐞️👾️🐞️👾️🐞️
 #######################################
 #mov $7 , %rdi	  #this was literally the problem!❗️
 #mov $31 , %rdi	  #this was literally the problem!❗️
  mov  $63 , %rdi 
 mov $0 , %rsi	  #this was literally the problem!❗️
 
 
 
  #☑️  
    #wanna compare first 64 and last of 1rst and second block.
 #  cmp $2,  debug_0_loopcountpure(%rbp)
  # jne debug_1_loop
# cmp $1 , %rsi  
# cmp $64 , %rsi  
# jne debug_0_loop
   
 #  sub $48, %al
 #  mov %al , %bl
 #  call exit   #]these are correct, maybe its the next one...
 #  debug_1_loop:  
    #☑️   
 
######################
#this "a2d,etc"  is a bit confusing in retrospect but its pretty containerized 
#it was "in reverse so this is swapping it back

 	 store_result_1: 

 store_result_a:
 cmp $0 , %r10 
 jne store_result_b
 	mov %rax , d_sum(%rbp)
 	#mov %rax, %rbx #❌️
 	#call exit	#❌️#❌️ = 51 / 3 
 	

 	
 	jmp end_store_result_1
 store_result_b:
 
	 cmp $1 , %r10 
 	 jne store_result_c
 	mov %rax , c_sum(%rbp)
 	#mov %rax, %rbx #❌️
 	#mov %r10, %rbx #❌️
 	#call exit	#❌️
 	
 	jmp end_store_result_1
  store_result_c:	
 	 cmp $2 , %r10 
 	 jne store_result_d
 	mov %rax , b_sum(%rbp) #❌️
 	#mov %rax, %rbx #❌️
 	#mov %r10, %rbx #❌️
 	#call exit	#❌️
 	
 	
 jmp end_store_result_1
 
 
  store_result_d:	
 	 cmp $3 , %r10 
 	 jne end_store_result_1
 	mov %rax , a_sum(%rbp)

 	#mov %rax, %rbx #❌️
 	#mov %r10, %rbx #❌️
 	#call exit	#❌️
 	
 	
 
#😱️ 
 #######################################
end_store_result_1:
 	
 	#mov %al ,(%r10,%r10,1)  #goes in backwards! (its ok debug)
 	inc %r10   #loop 4 4 fill, can reuse when 4 is full
 	cmp $4 , %r10 #4 is 2 much
 	jne  start_reverse_ascii_in_only_1
 	
 	
############################################
#😱️ < V i believe the fact that this instantly goes 2 "4" then "0" 
#😱️ means it "works just fine"
mov $0,  %r10 
#😱️ 
#abcd_sum are all stored, you can transplant logic now
#
#

#🖨️
xor %rdx ,%rdx  
xor %rcx ,%rcx  
xor %rbx ,%rbx  
xor %rax ,%rax  	
					#🗡️ new order
mov d_sum(%rbp) , %edx 		#was 'a'	#🗡️ header (ignore)	
		#⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️aug24.halo264
		#mov d_sum(%rbp) , %edx 
		#if i use "RDX" will segfault... (when tryign 2 store in "ASCII" probably cuz ascii is way 2 smol... 
		#im thinking about storing/pulling/from a seperate file in the future, sounds like a pretty solid idea tbh 
		#then u can look at it if u need and the "STATE IS ALREADY SAVED..." 
		#also u can just make cpu write "such and such lines of data" altho that seems like over kill
		#lets get the rest of this working first tho pls. 
		#(💿️in the meantime just keep bumping up the mem that u need here <3 
		#❗️its gonna do same thing (even zero headed) when pulling DOWN from wire
		#❗️ (use eax.) till ur ready 2 have bigger mem , will deal with it them <3 
		
		#⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️
						
mov c_sum(%rbp),    %ecx 	#was 'b'##🗡️ instruction 

mov b_sum(%rbp) , %ebx #output##🗡️ input _ b 			 ___# <> __#_ 

mov a_sum(%rbp)  ,  %eax  # wasinstruction   #🗡️ input _ a    __#_ <> ___#


#😇️😇️😇️😇️😇️😇️😇️😇️😇️😇️😇️😇️😇️
#17 For, behold, I create new heavens and a new earth; and the #former things shall not be remembered, nor come into mind. Isaiah #65:17 

#😇️😇️😇️😇️😇️😇️😇️😇️😇️😇️😇️😇️😇️
#

 #⚡️🐏️⚡️🐏️⚡️🐏️⚡️🐏️⚡️🐏️⚡️🐏️⚡️🐏️
#################################
#✋️⚡️(HI-Z)✋️⚡️(HI-Z)✋️⚡️(HI-Z)✋️⚡️(HI-Z)


#jmp ram_drop_thru_2   #emergency skip all
#🎧️🎧️🎧️🎧️🎧️🎧️🎧️🎧️🎧️🎧️🎧️🎧️🎧️
 # so dont go out of abc order...
#🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️
#we NEVER move(d) anything "back onto wire" once retriving (so its like a capacitor in that way) w/e , it works. ]aug24
#🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️
wire_header_ab_2_vi:

wire_cmp_a_2_vi:

cmp  $3, %al ##💱️🍀️3️⃣️🍀️3️⃣️🍀️3️⃣️🍀️
je wire_cmp_b_2_vi
    cmp  $4, %al   #was $16
    jb    wire_cmp_b_2_vi  # or a to b in this case 

  #🪲️
 # jmp wire_cmp_b_2_vi
   #🪲️
   mov (%r12,%rax,1) , %al
   sub $48 , %al
 
   #cmp $'.' , %rax ("."=46 so = 254 now!)
   cmp $254 , %rax 
   jne wire_cmp_b_2_vi

  mov $3 , %rax 	#❣️

      wire_cmp_b_2_vi:
#call exit #👨🏻‍⚕️️()made it here 

#############

cmp  $3, %bl  ##💱️🍀️3️⃣️🍀️3️⃣️🍀️3️⃣️🍀️
je end_wire_header_ab_2_vi    
 
       cmp  $4, %bl
    jb    end_wire_header_ab_2_vi  # or a to b in this case 

#🪲️
 # jmp wire_cmp_b_2_vi
   #🪲️

   mov (%r12,%rbx,1) , %bl
      sub $48 , %bl  #cuz pulling OUT of top wire
      
   
  cmp $254 , %rbx 
   jne end_wire_header_ab_2_vi

     mov $3 , %rbx 	#❣️
    end_wire_header_ab_2_vi:
  

 
 #✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️

 #
 ##LAB = 🥼️

#resistor can go before all logic , if  "wires" are on "inputs"
#it can choose wire with lower input 2 "Go thrU" 
#this can let us make a "NOT" using "diodes" (or w/e) 
#THAT , would be "COOL EMULATION OF PHY NOW" but not necsariy ofc .
#🥼️
#🏷️so wold skip this 2 bottom (already 'did logic')🏷️
 #
 ##👾️👾️👾️👾️👾️👾️👾️👾️👾️👾️👾️👾️👾️
   ######################
   mult_yr_2_a_vi:  


   finished_2_vi:
   #📮️---------starts
inc %r11
#📮️(incs at bottom postal post;)

   #🤹‍♀️️🤹‍♀️️🤹‍♀️️🤹‍♀️️🤹‍♀️️🤹‍♀️️🤹‍♀️️🤹‍♀️️🤹‍♀️️🤹‍♀️️🤹‍♀️️
 #📌️
 
 
 
 #######################################
   logic_switch_2_vi:
  #🚨️🚨️🚨️🚨️🚨️🚨️🚨️🚨️🚨️🚨️🚨️🚨️TEMT🚨️🚨️🚨️🚨️🚨️
  #🚨️temp fix for thing thats happening in "tx-logic", fix it there actually 

  
  #for some reason naked "0010" are in as "254" v this fixes
  #but u may wanna know why later (its cuz wire pull is minusing)
  


 
 skip_this_0: 
   #🚨️🚨️🚨️🚨️🚨️🚨️🚨️🚨️🚨️🚨️🚨️🚨️🚨️🚨️🚨️🚨️🚨️🚨️
   #⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️

#⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️
   #🪄️🪄️🪄️🪄️🪄️🪄️🪄️🪄️🪄️🪄️
   
   
     cmp $3 ,%cl    #⁉️💱️
    je end_logic_2_vi
  
   cmp $8 , %cl   #💱️⁉️
   je phy_tx_2_vi
   
 #  cmp $9 , %cl   #💱️⁉️
 #  je phy_tx_2_fix
 

 
    cmp $11 , %cl   #💪️🎅️ (fat flex)
   je phy_diode_2_fix
 
    cmp $12 , %cl   #💪️🎅️ (fat flex)
   je phy_ground_2_fix
   
   #📥️
  #   cmp $14 , %cl   
  # je modem_in_2_vi
    #📥️
    #📤️
   #  cmp $15 , %cl   
   #je modem_out_2_vi
    #📤️
    
     #📥️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️
    cmp $16 , %cl 
    jae  modem_in_2_vi  #modem_expanded_io_division
    #📤️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️⚜️

     cmp $7 ,%cl   #0111
   je clock_logic_2_vi
   
   #changing this 2 "%cl" from "rcx" finally got it "thru"

cmp $5 ,%cl   
   je switch_logic_2_vi
#switch_state
cmp $6 ,%cl   
   je switch_2_logic_2_vi
#switch_state
   
   #🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️
#❗️this cancels out "nunq empty case" 
#❗️so u should put in 'NUNQ type logic, but
#TRansistor acutally wants blanks so... yea.
ab_valid_vars_check_2_vi: #
##############################
a_valid_var_check_2_vi:
#call exit
#falstead nand treats "off as 0" so... this should work "The same"
cmp $2 ,%al
jne b_valid_var_check_2_vi   
mov $0 ,%rax
#je end_logic_2_vi



b_valid_var_check_2_vi:
cmp $2 ,%rbx 
jne a_valid_var_check_2_3
mov $0 ,%bl
#je end_logic_2_vi
#####33333333333333333333333333333333333333333333333333333333333333333333
a_valid_var_check_2_3:
#call exit
#falstead nand treats "off as 0" so... this should work "The same"
cmp $3 ,%al #🍀️3️⃣️🍀️3️⃣️🍀️3️⃣️🍀️
jne b_valid_var_check_2_vi_3   
mov $0 ,%rax
#je end_logic_2_vi

b_valid_var_check_2_vi_3:
cmp $3 ,%rbx #🍀️3️⃣️🍀️3️⃣️🍀️3️⃣️🍀️
jne end_ab_valid_vars_check_2_vi
mov $0 ,%rbx
#je end_logic_2_vi
#####33333333333333333333333333333333333333333333333333333333333333333333
#########################
end_ab_valid_vars_check_2_vi:
#🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️🚧️
   
  
    cmp $9 ,%cl  #1001   #7 0111 
   je logical_nunq_2_vi # logical_nand_2
  
 #cmp $6,%rcx   
  # je truthtable_symetrix_tx_2_vi
  

#🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️
  start_logic_2_vi:
  

#🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️
clock_logic_2_vi:




#🚨️it seems like the logic doesn't run after the first tiem!?
#🚨️why? wats going on? 
##🚨️was only cus cl stead of rcx ... wtf 


 mov clock_edge(%rbp) , %al
 
#working but now wont scoop 2 wire? 
#could it be the same kind of problem? 
 


mov $3 , %rbx  #💱️🍀️3️⃣️🍀️3️⃣️🍀️3️⃣️🍀️

 jmp end_logic_2_vi
#🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️
#🔘️
switch_logic_2_vi:

#can check if clock is greater than one or w/e 
#what is default switch state? 
#mov clock_count(%rbp) , %rbx


 #check default switch state 
 #its gonna have "2" in it at first w/e just test it
 
# mov %rax , %rbx
 #call exit
  mov switch_state(%rbp) , %al
  sub $48 , %al 
#mov $2 , %rbx
mov $3 , %rbx   #💱️🍀️3️⃣️🍀️3️⃣️🍀️3️⃣️🍀️
 jmp end_logic_2_vi
 #🔘️
 #🔘️
switch_2_logic_2_vi:


  mov switch_state_2(%rbp) , %al
  sub $48 , %al 
#mov $2 , %rbx
mov $3 , %rbx #💱️🍀️3️⃣️🍀️3️⃣️🍀️3️⃣️🍀️
 jmp end_logic_2_vi
 #🔘️
 #☎️☎️☎️☎️☎️☎️☎️☎️☎️☎️☎️☎️☎️☎️☎️☎️
  #📥️
  #theres only "AL" because its coming out/going in  "stack"(no sweat)
  # is there in / out? how does time fx work? (do that first)
  
 modem_in_2_vi:
 
 #AUG30  HARD PART OVER , u just need 2 parse modem_filename_in
 #based on %rcx ;) ez pz <3 YW 
#💡️
 #how? U NEED 2 PARSE THE DEC 2 BINARY THEN MATCH 2 FILE (PUSHING ASCII)
 #ALSO IF ITS JMP CARY THEN U CAN RUN "mODEM OUT" 
#*U CAN PARSE DEC2BIN FIRST (THEY BOTH USE  BIN ;) 
#💡️
 movq %rax , rax_pos(%rbp)
  movq %rbx , rbx_pos(%rbp)

movq %rcx , rcx_pos(%rbp)
 movq %rdx , rdx_pos(%rbp)

 movq %rsi , rsi_pos(%rsp)

movq %rdi , rdi_pos(%rbp)


######################🍰️
movq %r8 , r8_pos(%rbp)
 movq %r9 , r9_pos(%rbp)

 movq %r10 , r10_pos(%rsp)

movq %r11 , r11_pos(%rbp)
#
movq %r12 , r12_pos(%rbp)
 movq %r13 , r13_pos(%rbp)

 movq %r14 , r14_pos(%rsp)

movq %r15 , r15_pos(%rbp)




#jmp mov_abcs_back #🍰️ gives "33" if removed (for a&b). otherwise nothing
#👩🏻‍🏭️keep testing jmp end 2 see when it "breaks
#💵️#💵️#💵️#💵️#💵️#💵️#💵️#💵️#💵️#💵️#💵️#💵️#💵️

#movq  $modem_filename_test  ,  %r15 #position in header is BAD 4 this w/e
#mov $ram_32_0 , %rax #]]on wtf

mov $modem_test, %rax

# movq $ram_64_0, %r15

xor %rdx , %rdx
#####################################################

dec_2_bin_loop_start_re: 
xor %rdi ,%rdi 
xor %rsi ,%rsi

mov $0 , %rsi 
 mov  $7 , %rdi 
#>>>>>>>>>>>>>>>>>>>>
ascii_sum_out_loop_re:
xor %rbx , %rbx

shr $1 , %rcx
jnc no_carry_d2b_re  #jc 

mov $'1' , %rbx # 1 is carry 
jmp end_carry_d2b_re 
no_carry_d2b_re : 
mov $'0' , %rbx

end_carry_d2b_re : 


add $9 , %rdi #⬆️for "directory of modem digit space..(can remove if / = same)
mov %bl , (%rax,%rdi,1) #literally didn't work with "RBX" lil endian
#🩺️

#🩺️
end_placement_0_re: 

sub $9 , %rdi #⬆️for "directory of modem digit space..(can remove if / = same)
#call exit


cmp $7 , %rdi
jne end_store_first_shift

movb %bl , %dl
#call exit
#if 0 its modem in , if 1 is modem out . use this 2 jump after. 
end_store_first_shift:
###########
dec  %rdi 
inc  %rsi 



cmp $0 , %rdi
jge ascii_sum_out_loop_re
#💵️#💵️#💵️#💵️#💵️#💵️#💵️#💵️#💵️#💵️#💵️#💵️
#call exit


#cmpb $48 , %dl    #why 48 wut is that? = 'odd or even jnc'
cmpb $48 , %dl

movq $3 , %rax #🎂️ just 2 be clean 

jne modem_out_2_vi



#📤️#📤️#📤️#📤️#📤️#📤️#📤️#📤️#📤️#📤️#📤️#📤️#📤️#📤️

#  jmp mov_abcs_back #🍰️ WTF !?! 
#mov %r11 , %rbx 
  #  call exit 
#this makes it so "NOTHING DROPS 2 CLI ANYMORE..." why? 

   start_modem_open_1:
   
   mov $modem_test, %rdi #🚩   ##🛍️ 🔛address of output filename]ON
   mov $O_RDONLY, %rsi   
      # mov $O_CREAT_WRONLY_TRUNC, %rsi
    mov $0666, %rdx                    
    mov $SYS_OPEN, %rax 

    syscall #🏕️
    
    #mov %r11 , %rbx  #NOPE ! r11 is 70? now lol nope. save it. 
    #call exit   #Just save em all who has the time..
  #jmp mov_abcs_back #🍰️
wrap_modem_open_1:
    mov %rax, modem_in_stack_pos(%rbp)  #file addr...(not data)
    					#data will LOAD in $test_DATA



start_load_buff_modem_1:
# 🕹️
###BEGIN MAIN LOOP###
modem_load_loop_begin_1:
    ###READ IN A BLOCK FROM THE INPUT FILE###
    movq  $SYS_READ, %rax       #read
    movq  modem_in_stack_pos(%rbp), %rdi  #from this file
    movq  $test_DATA, %rsi    #to this buffer
    movq  $test_SIZE, %rdx    #up to this count
    syscall                     #size of buffer read is returned in %rax
    
  #
  
  
  #🔎️🔎️🔎️🔎️🔎️🔎️🔎️🔎️🔎️
  #QLU ? just take in "1char" first (then u can scale that up once u prove it works (ifu,want) <3
  #🔎️🔎️🔎️🔎️🔎️🔎️🔎️🔎️🔎️
  #📥️
 start_data_to_stack: 
     mov $test_DATA, %rax
    mov $test_SIZE, %rbx  #this was off 
    
    cmp $0, %rbx
    je end_data_to_stack
    
      mov $0, %rdi
    mov $0, %rsi
  #🎛️   
    movq $'3' , modem_in_single(%rbp)  # clear out if no new values
  mov  modem_in_count(%rbp) , %rdi #optional 4 stream
    

    mov (%rax,%rdi,1), %rbx  # test pulling out of modem
#call exit  #just run it , we can fix it if its brok m8 ]ON!(first try!)🎖️

mov %bl , modem_in_single(%rbp)

 #mov modem_in_single(%rbp) , %rbx
# call exit
 inc %rdi
 mov %rdi , modem_in_count(%rbp)
 

 #📮️

  end_data_to_stack:
  #📥️
 #☎️




 #nothing is going 2 be in modem "first pass" 
 #unless u want it 2 , then just do it as external fx first.
 # i dont see a problem taking 3 u need 2 set it mt neway

 movq modem_in_single(%rbp) , %rax
 
 
#🎂️
  cmp $48 , %rax  
  jae end_sanitize_0
  mov $'3' , %rax 
  
  end_sanitize_0:
 #🎂️
 
  sub $48 , %rax  
  
 
  
  #what if its a bad , or empty characteR ? ??? 
  #mov %rax , %rbx  
 #call exit
 
  mov clock_count(%rbp) , %rcx #🪲️
  cmp $1 , %rcx  #0
  jl end_clk_debug_0
  mov %rax , %rbx
 # call exit
 end_clk_debug_0:
 
 
 
# jmp mov_abcs_back #🍰️
 mov_abcs_back: 
  
#movq rax_pos(%rbp), %rax #value 2 print (save it)

#xor %rax, %rax
#xor %rbx, %rbx
#xor %rcx, %rcx
#xor %rdx, %rdx
#xor %rsi, %rsi
#xor %rdi, %rdi

#movq rax_pos(%rbp), %rax
movq rbx_pos(%rbp), %rbx
movq rcx_pos(%rbp), %rcx
movq rdx_pos(%rbp) , %rdx 
movq  rsi_pos(%rbp) , %rsi 
movq  rdi_pos(%rbp) , %rdi 
 #🍰️
movq  r8_pos(%rbp), %r8
 movq r9_pos(%rbp), %r9

 movq r10_pos(%rsp), %r10

movq r11_pos(%rbp), %r11

movq  r12_pos(%rbp), %r12
 movq r13_pos(%rbp), %r13

 movq r14_pos(%rsp), %r14

movq r15_pos(%rbp), %r15
  
 #🍰️
#movq %rax ,  %rbx
#call exit 
  movq $3 , %rbx
  jmp end_logic_2_vi

 end_modem_in_2_vi:
 #📥️
 

 
 
    #📤️#📤️#📤️#📤️#📤️#📤️#📤️#📤️#📤️#📤️#📤️#📤️#📤️#📤️#📤️#📤️
 modem_out_2_vi:

 start_modem_out_open:
 ####################################
# mov $modem_filename_out, %rdi #🚩 
 mov $modem_test, %rdi  
#########################
    mov $0666, %rdx                     # perms
    mov $0x441, %rsi  
    mov $SYS_OPEN, %rax #🚩 #💰 file addr stored in rax after syscall
    syscall #🏕️
    
    mov %rax, modem_out_stack_pos(%rbp)  #file addr...(not data)
    
    end_modem_out_open:
  #♑️#♑️# 
    #####################
    
#💞️
    mov  rax_pos(%rbp) , %rax      
    add $48 , %rax 
    mov $ascii_sing  , %rbx  
   mov  $0 , %rsi 
    mov %rax , (%rbx,%rsi,1)   
  #🖨️
   mov  $ascii_sing , %rsi  # this works... i think it has 2 be in ascii buffer
    
    movq  modem_out_stack_pos(%rbp), %rdi   #to this file s   
    movq  $1, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to

    syscall 
  #🖨️
  ########################💔️
  
 end_modem_out_2_vi:
  mov $3 , %rbx
  
  
  #put rsi rax and rdx back pls
 #movq  0(%rsp), %rax #value 2 print (save it)
# movq  8(%rsp) , %rdx 
# movq  16(%rsp) , %rsi 
 # movq  24(%rsp) , %rdi
 
movq rax_pos(%rbp), %rax #value 2 print (save it)
movq rdx_pos(%rbp) , %rdx 
movq  rsi_pos(%rbp) , %rsi 
movq  rdi_pos(%rbp) , %rdi 
  
 # pop -8(%rsp)
 # pop -8(%rsp)
 # pop -8(%rsp)
 # pop -8(%rsp)
 #  pop -8(%rsp)
 #if i never pop stack it never breaks, i m not interested (keeps growing)
 #just push 2 a rbp address pls. fuck that. 
  
#🛗️

 
  jmp end_logic_2_vi
  #📤️
  #all of ur "archs" can have "secret io code" 2 hook in 2 this EXOIO
  #so i think u should "CLOSE THE FILES " when ur done with them
  #so it wont slow down ur "VMCPU's
  #secwise? yes this is 1way a fab maker could "own" all manufactured devices
 #nano meters are pretty smol these days...im sure thers a bit of space.
 
#divide if even its in if odd its output . 
#use specific # when making/choosing "string"
  #☎️☎️☎️☎️☎️☎️☎️☎️☎️☎️☎️☎️☎️☎️☎️☎️
#🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️
#🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️
 
 
 #🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️
#u should probably do "3ST.Buffer BACKWARDS (use _0 <43st's (0_ < is used 4 passthru noting)#🇺🇸️
#🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️🇺🇸️
phy_tx_2_vi: 

#mov $91 , %bl
 
#call exit #❌️

phy_tx_fx_2_vi: 
pt_tx_fx_2_vi: 
pt_cmp__:
#=0
#call exit

# cmp $2 , %al
# jne pt_cmp00
 
# call exit #is working...



 cmp $2 , %bl
 jne pt_cmp00
 cmp $2 , %al
 jne pt_cmp00
  #is no longer triggering 
 #call exit #is working...
 
 #no fx needed is already "_"
 #mov $0 , %al
 mov $0 , %al #🦌️🪳️ 🦋️
# mov $2 , %al # 🦋️ 🇺🇸️ (not doing , smart ass? 
 
 jmp end_pt_fx_2_vi

pt_cmp00:

#=0🐜️ (does "_" work? (no¿¡⸘ but were covered with "11";)
#=1🦌️
cmp $0 , %bl
 jne pt_cmp0_
 cmp $0 , %al
 jne pt_cmp0_
 
#mov $0 , %al🐜️
mov $0 , %al #🦌️🪳️
 jmp end_pt_fx_2_vi
 

################NOT
 pt_cmp_0: #🦢️
#=0
cmp $2 , %bl
 jne pt_cmp1_
 cmp $0 , %al
 jne pt_cmp1_
 
#mov $0 , %al
#mov $1 , %al #🦌️🪳️🦋️ 
mov $1 , %al #🇺🇸️🇺🇸️🇺🇸️ #🎆️
 
 jmp end_pt_fx_2_vi
 
  pt_cmp_1: #🦢️
#=1
cmp $2 , %bl
 jne pt_cmp11
 cmp $1 , %al
 jne pt_cmp11
 
#mov $1 , %al
#mov $0 , %al #🦌️🪳️🦋️ 
mov $2 , %al #🇺🇸️🇺🇸️🇺🇸️#🦢️
jmp end_pt_fx_2_vi
############################

#####################TRISTATE Z
 pt_cmp1_: #🦢️
#=1 🐜️
#=0🦌️

cmp $1 , %bl
 jne pt_cmp_1
 cmp $2 , %al
 jne pt_cmp_1
 
#mov $1 , %al
mov $2 , %al #🦌️🪳️ #🦢️
jmp end_pt_fx_2_vi

 pt_cmp0_: #🦢️
#=1 #😖️dont like, can we fix somehow?(iterate/harden w/o breaking?)
#=1 #🦌️(means e=z)

cmp $0 , %bl
 jne pt_cmp_0
 cmp $2 , %al
 jne pt_cmp_0
 
#mov $1 , %al
 mov $1 , %al #🦌️🪳️
 
 jmp end_pt_fx_2_vi
 ################################

 pt_cmp11:
#=2
cmp $1 , %bl
 jne pt_cmp10
 cmp $1 , %al
 jne pt_cmp10
 
#mov $2 , %al
mov $2 , %al #🦌️🪳️🦋️ 
jmp end_pt_fx_2_vi

 pt_cmp10:
#=2
cmp $1 , %bl
 jne pt_cmp01
 cmp $0 , %al
 jne pt_cmp01
 
#mov $2 , %al
mov $0 , %al #🦌️🪳️🦋️ 
jmp end_pt_fx_2_vi


 pt_cmp01:
#=1
cmp $0 , %bl
 jne pt_cmp33
 cmp $1 , %al
 jne pt_cmp33
 
#mov $1 , %al
mov $0 , %al #🦌️🪳️🦋️ 
jmp end_pt_fx_2_vi

#done
#🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️🚬️
#🚬️🐝️🫁️
 pt_cmp33:
 
cmp $3 , %bl
 jne end_pt_cmp33
 cmp $3 , %al
 jne end_pt_cmp33
# call exit

mov $3 , %al #🚬️  #🚭️dosen't affect either latch
jmp end_pt_fx_2_vi

end_pt_cmp33:

 pt_cmp30:
 
cmp $3 , %bl
 jne end_pt_cmp30
 cmp $0 , %al
 jne end_pt_cmp30
 #call exit
mov $1 , %al #🚬️ 
jmp end_pt_fx_2_vi

end_pt_cmp30:

 pt_cmp31:
cmp $3 , %bl
 jne end_pt_cmp31
 cmp $1 , %al
 jne end_pt_cmp31
 
 #call exit
mov $0 , %al #🚬️ 
jmp end_pt_fx_2_vi

end_pt_cmp31:

#🚬️🐝️🫁️
 pt_cmp03:
cmp $0 , %bl
 jne end_pt_cmp03
 cmp $3 , %al
 jne end_pt_cmp03
 
mov $1, %al #🚬️ 
jmp end_pt_fx_2_vi

end_pt_cmp03:

 pt_cmp13:
cmp $1 , %bl
 jne end_pt_cmp13
 cmp $3 , %al
 jne end_pt_cmp13
 
mov $0 , %al #🚬️ #💱️(same as old)
jmp end_pt_fx_2_vi

end_pt_cmp13:
#################################
 pt_cmp3_:
cmp $3 , %bl
 jne end_pt_cmp3_
 cmp $2 , %al
 jne end_pt_cmp3_
 		
			#💱️(same as old)(affects blank flipflop!)
mov $1 , %al #🚬️   #🚭️dosen't affect either latch
jmp end_pt_fx_2_vi


end_pt_cmp3_:

#call exit 

 pt_cmp_3:
cmp $2 , %bl
 jne end_pt_fx_2_vi
 cmp $3 , %al
 jne end_pt_fx_2_vi
 			#💱️(same as old)(affects blank flipflop!)
mov $1 , %al #🚬️   #🚭️dosen't affect either latch
jmp end_pt_fx_2_vi


#🚬️🐝️🫁️
#🥠️🥠️🥠️🥠️🥠️

end_pt_fx_2_vi:
#end_phy_tx_2_vi:
   mov $3, %bl        #💱️
   jmp end_logic_2_vi
   #🥠️🥠️🥠️🥠️🥠️
#tristate buffer 74HC125 (inverted)
#  c a d
#|0|1|1    Z & 1 = Z =zc0  =1
#|0|0|0    Z & 0 = 1 = 1c0 =0 
#|1|0|Z    Z & 0 = 1 = 1c1 =Z  
#|1|1|Z	   Z & 1 = Z =zc1  =Z   
	
#🍀️
#4State BJT Transistor Small Signal Model Truth Table	
#|   a   |   b   |  out  |
#|   0   |   0   |   0   |
#|   0   |   1   |   0   |
#|   1   |   0   |   0   |
#⚡️Hi-Impedance
#|   1   |   Z   |   Z   | 
#|   0   |   Z   |   1   |

#|   Z   |   1   |   Z   |
#|   Z   |   0   |   1   | 
#
#|   1   |   1   |   Z   |
#|   Z   |   Z   |   0   |
#⚡️
##🚬️Small Signal Cutoff
#|   3   |   3   |   3   |
#|   3   |   0   |   1   |
#|   3   |   1   |   0   |
#|   0   |   3   |   1   |  
#|   1   |   3   |   0   |
#|   3   |   Z   |   1   |                 	 
#|   Z   |   3   |   1   |
#_🚬️_)🚬️_)🚬️_)🚬️_)🚬️_)🚬️_)🚬️_)
#🍀️
#👑️
#all this code works, u can just do diode are ur leisure or w/e at least the TT is on record etc <3
#when their not rap u u should do it...(since there the ones who wanted it ;) [mult-modem will get done first i imagine]
#LITERALLY THAT. = 21:11 = 🔍️ = Q

#📆️
#and by not rap i mean longer than 5 minutes or a day or w/e that they wanna get something...

#if they did show GENUINE (non manipulative) growth and control over marathom period(taken root)
#i woud be a fool not 2 reward them . that would be  abig deal...

#>they just blew the speakers on my computer somehow...
#cuz i gave an honest and positive opinino about "itzy's sneakers' : / 
#❗️MOVE everything Up 1 "so we have space for a "LOWz" or w/e else 
#❗️u dont have 2 impliment it just move it . one day w/e grep is ez. 
#❗️not in mood right now cuz they have been harassing me with fake repairs and trying 2 push back my MBA 
#i moved spots for clock and nunq 4 lowz no biggie if u want for now u can convert "111" 2 "11"
#and just treat them the same w/e #im gonna super ship but easily swapping out bins first
#📆️
#🔍️🔍️🔍️🔍️🔍️🔍️🔍️🔍️🔍️🔍️
#
#👑️
#🎲️🎲️🎲️🎲️🎲️🎲️🎲️🎲️🎲️

 #  cmp $11 , %cl   #💪️🎅️ (fat flex)
 phy_diode_2_fix:
 
#👨🏿‍🎤️
 dio_cmp00:
 
 
  cmp $0 , %al
 jne end_dio_cmp00
cmp $0 , %bl
 jne end_dio_cmp00


mov $0 , %al #🚬️  #🚭️dosen't affect either latch
jmp end_diode_fx_2_vi

end_dio_cmp00:
#👨🏿‍🎤️
 dio_cmp01:
 #👩🏿‍🎤️
 #👨🏿‍🎤️
 dio_cmp10:
 
 
  cmp $0 , %al
 jne end_dio_cmp00
cmp $1 , %bl
 jne end_dio_cmp00


mov $1 , %al #🚬️  #🚭️dosen't affect either latch
jmp end_diode_fx_2_vi

end_dio_cmp10:
#👨🏿‍🎤️
 dio_cmp02:
 #👩🏿‍🎤️
 
 ##########
 #🥠️🥠️🥠️🥠️🥠️
 end_diode_fx_2_vi:
  mov $3, %bl        #
   jmp end_logic_2_vi
   #🥠️🥠️🥠️🥠️🥠️
 
 #DIODE#

#ASYM DIODE = REAL
#B 	A	D 
#|   0   |   0   |   0   | 
#|   0   |   1   |   1   | 
#|   1   |   0   |   0   | 
#|   1   |   1   |   1   | 

#if u leave it like this and do "NUNQ PRE " thats will "DO IT"
#and is easy so is or
#not is the real challenge

#u should ship diode with "64"(maybe cascadding modem as well) 


#can easily make "OR& GATE" with these...
 #🎲️🎲️🎲️🎲️🎲️🎲️🎲️🎲️🎲️
 
 #⛱️⛱️⛱️⛱️⛱️⛱️⛱️⛱️
  #  cmp $12 , %cl   #💪️🎅️ (fat flex)
 #  je 
 phy_ground_2_fix:

  gnd_cmp33:
 
  cmp $3 , %al
 jne end_gnd_cmp33
cmp $3 , %bl
 jne end_gnd_cmp33

jmp end_logic_2_vi

end_gnd_cmp33:
 mov $0 , %al #🚬️  #
   mov $3, %bl        #
   

   jmp end_logic_2_vi
#⛱️⛱️⛱️⛱️⛱️⛱️⛱️⛱️
#❗️this ground logic is actually bad. i think the TX already "grounds itself"
#if its given as a 3. anyways "BIT WORKS w/o any weird side effects recursively so...)
#i just didn't flesh this out yet but if u treid 2 use it thats not even how u want
#groudn 2 work i would just pass w/e volt unless its both 3's ? even then 
#i may pass a zero w/e 
#GROUND
#|   X   |   X   |   3   | <could just use this since it DOESNT MATTER AT ALL
############################# < V im not sure that thats true... (unless were tryign 2 make moreacc tx...?) ^ this first..
#(OR V This actually makes more sense 4 ur "FX" 
#|   3   |   3   |   3   | 

#| X-3   | X-3   |   0   | (so otherwise readout "0" (hot ground/lo)

#option - u could optionally tweak this , since it doesnt' "matter" at ALL...
#|   3   | X-3   |   0|3?   | 
#| X-3   |   3   |   0|3?   | 

#these are cute [becase no truth tables exist for them...]
# but i have a feelling a "PNR tape out isn't gonna use EITHER of these..."
#especially with the facts that DIODE Tx is diff than "TX=TT" 





#👾️🧮️👾️🧮️👾️🧮️👾️🧮️👾️🧮️👾️🧮️👾️🧮️👾️🧮️
#i swapped "a&b" but somehow this still works so w/e 
#we have bigger problems. this is ez. its already on.
 #@@@@@@@@@@@@@@@@@@@@@
   logical_nunq_2_vi: 
  # call exit
   #👨🏽‍🚀️
   #new logical nand (just make using "cmp $0 & $1 )
   #(cuz the arch is doing somethingweird(will try %al...
    #👨🏽‍🚀️
 #   sub $'0' , %rax   
  #sub $'0' , %rbx   
  
  unq_fx_2_vi: 
  
  
 cmp_a0b0_vi:
 cmp $0 , %al
 jne cmp_a0b1_vi
 cmp $0 , %bl
 jne cmp_a0b1_vi
 #call exit
 # mov $1 , %al  #NO , XOR HAPPENS AT "NUN"
 jmp nun_fx_2_vi
 #done cmp_a0b0 =0
 
 cmp_a0b1_vi:

  cmp $0 , %al
 jne cmp_a1b0_vi
 cmp $1 , %bl
 jne cmp_a1b0_vi
 jmp nun_fx_2_vi

 #done cmp_a0b1 = 0
 
 cmp_a1b0_vi:
 
   cmp $1 , %al
 jne cmp_a1b1_vi
 cmp $0 , %bl
 jne cmp_a1b1_vi
 
 mov %bl , %al
 jmp nun_fx_2_vi

 #done cmp_a1b0 = 0
 
 cmp_a1b1_vi:
 #al is 1 , ur done already

 #done cmp_a1b1 = 1
  
   nun_fx_2_vi: 
     xor $1 , %al
# add $'0' , %al 
 # add $'0' , %bl
  
 
   end_logical_nunq_2_vi: 
   mov $2, %rbx
   jmp end_logic_2_vi
     #👨🏽‍🚀️
   #🔥️  #also they die and burn in hell soon 4 sending me 2 shower for no reason <DRU ^^^^^^^^^^^^^^^^^
   
#https://kobzol.github.io/davis/
#riscv
#wanted to see if they booth do that (simulated)
     
     #&
#|   a   |   b   |  out  |
#|   0   |   0   |   0   |
#|   0   |   1   |   0   |
#|   1   |   0   |   0   |
#|   1   |   1   |   1   |







#👾️🧮️👾️🧮️👾️🧮️👾️🧮️👾️🧮️👾️🧮️
#🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️
#👾️🧮️👾️🧮️👾️🧮️👾️🧮️👾️🧮️👾️🧮️
#🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️
end_logic_2_vi:
#🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️🤖️
 #
 #
 #
 
 
#🪱️##🪱️##🪱️#(rewriting this part is ackshually ez)

#🥷️🥷️🥷️🥷️🥷️🥷️🥷️
	cmp  $4 , %dl   
	jb    end_scoop_wr_d_2_vi  #$4


#mov $'1', (%r12,%rdx,1)
#jmp end_scoop_wr_d_2_vi
#⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️aug24.halo264
		#mov d_sum(%rbp) , %edx 
		#if i use "RDX" will segfault... (when tryign 2 store in "ASCII" probably cuz ascii is way 2 smol... 
		#im thinking about storing/pulling/from a seperate file in the future, sounds like a pretty solid idea tbh 
		#then u can look at it if u need and the "STATE IS ALREADY SAVED..." 
		#also u can just make cpu write "such and such lines of data" altho that seems like over kill
		#lets get the rest of this working first tho pls.
		#(💿️in the meantime just keep bumping up the mem that u need here <3 
		#⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️⚠️💾️

 ######################4 a&b 
 stor_rax_wr_2_vi:  
 cmp $3 , %al    
 je stor_rbx_wr_2_vi

  add $48 , %al

#movb $'_', (%r12,%rdx,1)
 mov %al , (%r12,%rdx,1)  #< LITERALLY CAUSE OF SEGFAULT
 mov $'_' , %rdx

 jmp end_scoop_wr_d_2_vi

##################
 stor_rbx_wr_2_vi: 
   cmp $3 , %bl 
 je end_scoop_wr_d_2_vi
 

  add $48 , %bl
 mov %bl , (%r12,%rdx,1) 
 #movb $'_', (%r12,%rdx,1)
 mov $'_' , %rdx
 
 
#🍀️3️⃣️🍀️3️⃣️🍀️3️⃣️🍀️
#🪱️##🪱️##🪱️#
end_scoop_wr_d_2_vi:

#🪱️##🪱️##🪱️#
#📌️
#👖️👖️👖️👖️👖️👖️👖️👖️👖️👖️👖️👖️👖️👖️👖️👖️👖️👖️👖️👖️👖️👖️👖️
patch_drop_ab_2_vi:

#📮️
dec %r11  #u may want this after logic but 
 	#when im debuging it pre everythign it decs 2 far bac <3
#📮️

#🎣️🎣️🎣️🎣️🎣️🎣️🎣️🎣️🎣️🎣️🎣️
cmp $'_', %rdx
  je end_post_logic_check_ab_2_vi
 ############DEBUG#############
  jmp post_logic_check_ab_2_vi
  post_logic_check_d_2_vi:
# cmp  $2, %rdx ##🍀️3️⃣️🍀️3️⃣️🍀️3️⃣️🍀️
# cmp  $3, %rdx


 #  add $48 , %dl
# mov %dl, (%r13,%r11,1) 
 #mov %dl, (%r13,%r14,1) # <fake for debug (does nothing) but why , u know? 

 #inc %r11
 
 #mov %rdx , %rbx 
 #call exit
 
 
 
#>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 post_logic_check_c_2_vi:
#cmp  $2, %rcx ##🍀️3️⃣️🍀️3️⃣️🍀️3️⃣️🍀️
#cmp  $3, %rcx
#  je end_patch_drop_ab_2_vi
# add $48 , %cl
#mov %cl, (%r13,%r11,1) 	
#mov %cl, (%r13,%r14,1) 

  inc %r11
  ############################

end_patch_drop_ab_2_vi:

post_logic_check_ab_2_vi:  


	
 
  post_logic_check_b_2_vi:
   #🍰️
  #call exit 
  
# cmp  $2, %rbx #🍀️3️⃣️🍀️3️⃣️🍀️3️⃣️🍀️
 #je end_post_logic_check_ab_2_vi
# cmp  $3, %rbx
   add $48 , %bl
  mov %bl, (%r13,%r11,1) 
#  mov %bl, (%r13,%r14,1) 
 inc %r11

post_logic_check_a_2_vi:
	
# cmp  $2, %rax ##🍀️3️⃣️🍀️3️⃣️🍀️3️⃣️🍀️
# je post_logic_check_b_2_vi
 #cmp  $3, %rax
#  je post_logic_check_b_2_vi
   add $48 , %al
mov %al, (%r13,%r11,1) 
#mov %al, (%r13,%r14,1) 
  inc %r11


jmp end_post_logic_check_ab_2_vi

#⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️
#1count segfaults prior 2 this ! 
#⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️⌚️
#🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️
end_post_logic_check_ab_2_vi:



#✋️⚡️(HI-Z)✋️⚡️(HI-Z)✋️⚡️(HI-Z)✋️⚡️(HI-Z)
#🎡️#🎡️#🎡️#🎡️#🎡️#🎡️#🎡️#🎡️#🎡️#🎡️#🎡️#🎡️#🎡️#🎡️#🎡️#🎡️#🎡️#🎡️

#✂️✂️✂️✂️✂️✂️#🪱️
		loop_if_2_vi:
		
		#inc %r11 # dont inc this. r11 is clean now. lets keep it that way
		
		

  incq debug_0_loopcountpure(%rbp) #this counts "blocks of 4"w/o "r16+" its good 
         #🎖️

jmp  start_reverse_ascii_in_only_1  
#8️⃣️⏱️
#🥼️]🥼️]🥼️]🥼️]🥼️
# #🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️

 #🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️
 #clck fxx begin
 #🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️
 
 
 #if u can avoid u may not wanna "read-jump all the way down here"
 clock_fx_start_0: 
 
 
 
 #🖨️☎️

 #📤️☎️📤️☎️📤️☎️
 
    #📥️☎️📥️☎️📥️☎️  
    #📤️☎️📤️☎️📤️☎️📤️☎️📤️☎️📤️☎️📤️☎️
 
 #📤️☎️📤️☎️📤️☎️📤️☎️
 #💵️ #🖨️💵️ #🖨️💵️ #🖨️💵️ #🖨️💵️ #🖨️💵️
    mov $ram_256_cli_0, %rsi    #👜💰
     mov ST_LOG_OUT_0(%rbp), %rdi  #🖨️💰 
     
 #movq  $256, %rdx             #buff size
movq  %r11, %rdx 
    mov $SYS_WRITE, %rax	#🖨️ 💸
    syscall #🏕️
  

     #♑️#♑️# 
    
    mov  $newline_out , %rsi      #this buffer 
        movq  ST_LOG_OUT_0(%rbp), %rdi   #to this file s   
    movq  $1, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to

    syscall
      #♑️#♑️#
     #🦬️ (
    	
    	 mov $clock_out_8bit  , %rdx  
    	 
   movq $1, %rax #lasttime i checked \n only takes 1 char ;) 
   mov clock_count(%rbp) , %rbx
   add $48 , %rbx
    mov %bl, (%rdx,%rax,1) #🦞️ < 
                  #u can actually digit this out the way "x86ioperm" does
                  #and u can even tie this 2 sys clock somehow if u want
 
  mov $clock_out_8bit  , %rdx  
  
    
   mov clock_edge(%rbp) , %rbx
    movq $0, %rax #lasttime i checked \n only takes 1 char ;) (254. 255 = 1
   add $48 , %rbx
    mov %bl, (%rdx,%rax,1)   
            
        movq  $clock_out_8bit, %rsi      #this buffer
   
    movq  $8, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_LOG_OUT_0(%rbp), %rdi   #to this file s
    
    syscall
    #♑️#♑️#
     #♑️#♑️# 
    
    mov  $newline_out , %rsi      #this buffer 
        movq  ST_LOG_OUT_0(%rbp), %rdi   #to this file s   
    movq  $1, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to

    syscall
      #♑️#♑️#
    
    
     #🖨️💵️ #🖨️💵️ #🖨️💵️ #🖨️💵️
  #____________________________
 #🥼️]🥼️]🥼️]🥼️]🥼️
  #🖨️
 #movq  $BUFFER_DATA, %rsi      #this buffer
  # movq $BUFFER_SIZE, %rax #replace size?
   # movq  %rax, %rdx             #buff 2 
   #movq  $SYS_WRITE, %rax        #write to
   # movq  ST_FD_OUT(%rbp), %rdi   #to this file s
   # syscall
         #♑️#♑️#♑️#♑️#♑️#♑️
    # mov  $newline_out , %rsi      #this buffer 
    #movq  $1, %rdx             #buff size
    #movq  $SYS_WRITE, %rax        #write to
    #movq  ST_FD_OUT(%rbp), %rdi   #to this file s   
    #syscall
    #♑️#♑️#♑️#♑️#♑️#♑️#♑️
 #🖨️(can also be sending 2 a log file "EZ" 
 #🖨️(how ? write 2 a ram a keep track of count + \n
 
 
  movq $ram_1024_v_0, %rsi      #this buffer
    movq  $1024, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
     #♑️#♑️#♑️#♑️#♑️#♑️
     mov  $newline_out , %rsi      #this buffer 
    movq  $1, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s   
    syscall
    #♑️#♑️#♑️#♑️#♑️#♑️#♑️
 #♑️(this is good 2 go , run it) 
 
 print_256_min_count_0: 
 #
  mov  $ram_256_cli_0 , %rsi      #this buffer 
    movq  $256, %rdx             #buff size
   #movq  %r11, %rdx   
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s   
    syscall
    #♑️#♑️#
     mov  $newline_out , %rsi      #this buffer 
    movq  $1, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s   
    syscall
    #♑️#♑️#
    
  
   
    
 #🖨️
  #############NOT CLK LOGIC, JUST A LOGOUT! #########
 #🦬️ (
    	
    	 mov $clock_out_8bit  , %rdx  
    	 
   movq $1, %rax #lasttime i checked \n only takes 1 char ;) 
   mov clock_count(%rbp) , %rbx
   add $48 , %rbx
    mov %bl, (%rdx,%rax,1) #🦞️ < 
                  #u can actually digit this out the way "x86ioperm" does
                  #and u can even tie this 2 sys clock somehow if u want
 
  mov $clock_out_8bit  , %rdx  
  
    
   mov clock_edge(%rbp) , %rbx
    movq $0, %rax #lasttime i checked \n only takes 1 char ;) (254. 255 = 1
   add $48 , %rbx
    mov %bl, (%rdx,%rax,1)   
            
        movq  $clock_out_8bit, %rsi      #this buffer
   
    movq  $8, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
        
        
            #⏰️
            # clock_out_8bit:
		#.ascii "::::::::"
		#put this anywhere will handle it later
		  #⏰️may use sys clock w/e 
                   #⏰️  #⏰️  #⏰️  #⏰️  #⏰️ 
                    #♑️#♑️#♑️#♑️#♑️#♑️
      #♑️#♑️#♑️#♑️#♑️#♑️
     mov  $newline_out , %rsi      #this buffer 
    movq  $1, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s   
    syscall
    #♑️#♑️#♑️#♑️#♑️#♑️#♑️
                  
#🦬️

 #🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️
 
mov clock_edge(%rbp) , %rax 
 
 xor $1 , %al
 mov %rax , clock_edge(%rbp)

 mov clock_count(%rbp) , %rbx
 inc %rbx
 mov %rbx , clock_count(%rbp)
 
 
 #################ALT CLOCK = "0-,0+,1-,1+"
 ###############could "run both" and have variable in fx 2 choose ? w/e 

 
 ################################
  add $48 , %rbx

  mov  %rbx , %rsi      #this buffer 
    movq  $8, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s   
    syscall
    
     #♑️#♑️#
     mov  $newline_out , %rsi      #this buffer 
    movq  $1, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s   
    syscall
    #♑️#♑️#
    
    
    
 
#➿️#➿️#➿️#➿️#➿️#➿️#➿️#➿️#➿️
 cmp $53 , %r9  # %rbx 
je re_run_entire_with_new_clock_0
#➿️#➿️#➿️#➿️#➿️#➿️#➿️#➿️#➿️#➿️#➿️#➿️#➿️#➿️#➿️#➿️


 #🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️🕰️🖨️
#8️⃣️⏱️
 user_clk_control_0: 
#prompt_str1:
 #   .ascii "Enter first number: "

mov  $prompt_str1 , %rsi      #this buffer 
    movq  $STR1_SIZE, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s   
    syscall

   movq  $SYS_READ, %rax       #read
   mov  $STDIN, %rdi
    movq  $input1, %rsi    #to this buffer
    movq  $2, %rdx    #up to this count
    syscall   
     
     mov $0 , %rdi #r9
    mov (%rsi ,%rdi,1), %bl  # lower byte of rcx

mov %rbx, %r9
#call exit 
                               #cmp print state Break,
                               
                #🔘️
       #############                      #switch off
   store_switch_zero:
   #use stack 2 store switch result                          
cmp $49 , %r9  # %rbx 
jne store_switch_one
mov switch_state(%rbp)  ,%rax

#mov %rax , %rbx 
#call exit

cmp $2 ,%rax
#cmp $'3' ,%rax 
#this should clue u in that 2 isn't "REAL" 
#we may fix that later when we drop 2's but its a review of
#entire codebase so dont think it will be "QUICK"
#part of the confusion is that XOR does 48&49 ;) <3 


jne switch_zero_xor
#call exit
mov $48 , %rax       #has 2 be 48=0 then jump (but y?)
mov %rax, switch_state(%rbp)  
jmp re_run_entire_with_new_clock_0

switch_zero_xor:
xor $1 , %rax
mov %rax, switch_state(%rbp)  
jmp re_run_entire_with_new_clock_0



	#########			 #switch on
 store_switch_one:
   #use stack 2 store switch result  
cmp $50 , %r9  # %rbx 
jne switch_zero_hiz
mov switch_state_2(%rbp)  ,%rax

cmp $2 ,%rax   #💱️⁉️
#cmp $'3' ,%rax 
jne switch_one_xor
mov $48 , %rax
mov %rax, switch_state_2(%rbp)  
jmp re_run_entire_with_new_clock_0

switch_one_xor:
xor $1 , %rax
mov %rax, switch_state_2(%rbp)  
jmp re_run_entire_with_new_clock_0

      ###############        
#jmp re_run_entire_with_new_clock_0

switch_zero_hiz:
 cmp $51 , %r9  # %rbx           
jne switch_one_hiz
movq $2, switch_state(%rbp)  
jmp re_run_entire_with_new_clock_0

switch_one_hiz:
cmp $52 , %r9  # %rbx 
jne switch_zero_cutoff
movq $2, switch_state_2(%rbp)  
jmp re_run_entire_with_new_clock_0


switch_zero_cutoff:
 cmp $56 , %r9  # %rbx           
jne switch_one_cutoff
movq $'3', switch_state(%rbp)  
#call exit
jmp re_run_entire_with_new_clock_0

switch_one_cutoff:
cmp $57 , %r9  # %rbx 
jne end_clock_compares
movq $'3', switch_state_2(%rbp)  
jmp re_run_entire_with_new_clock_0

##################


 #🛑️
# cmp $5?+- , %r92 <?  #🛑️ will just fall thru 2 exit, this is end of fx


end_clock_compares: 
                #🔘️  
jmp re_run_entire_with_new_clock_0
#8️⃣️⏱️
#🥼️]🥼️]🥼️]🥼️]🥼️
# #🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️🕰️
 #____________________________
 #🛑️
 #____________________________
 #🛑️
    end_a2b_1:
    
    mov  %r11 , %rbx 
    call exit
    
 popq  %rbp
    ret 

#🥼️🥼️
    
    
    
    #❎️#❎️#❎️#❎️#❎️#❎️#❎️#❎️#❎️#❎️#❎️
    
    
    
    #❎️#❎️#❎️#❎️#❎️#❎️#❎️#❎️#❎️#❎️#❎️
    
    
    
    
    #❎️#❎️#❎️#❎️#❎️#❎️#❎️#❎️#❎️#❎️#❎️
    
    
    
    
    
    
    
    
    
    
    
    #@⌨️⌨️⌨️⌨️⌨️⌨️⌨️⌨️⌨️⌨️⌨️⌨️
.globl kbd2ascii_io_0 
		#⌨️⌨️⌨️⌨️⌨️
kbd2ascii_io_0:
 pushq %rbp 
    movq  %rsp, %rbp
#_________________________
 start_kbd2ascii_0:  
 

 ###########
  
  
 movq $ram_64_0, %r8 #header goes first , ez
mov $0 , %r9 # ram-stack count
###############

#i dont think u wanna even look at bios way
#i think u need 2 use either "ps/2" or
#get the port # for kbd <-(scalable)

get_io_perm_0: 
 # ioperm(0x42, 32, 1)
    mov     $173, %rax  # ioperm() == 173 in x86_64
   # mov     $0x42, %rdi # from port 0x42 = 66
   mov     $0x0, %rdi
   # mov     $32, %rsi   # num ports, to port 0x62 = 98...
   mov     $0x0cdb , %rsi 
    mov     $1, %rdx    # enable
    syscall
    cmpl    $0, %eax
    je      start_kbd_0
    
     # ioperm() returned error,
    # write(1, ioperm_message, 13)
    mov     $1, %rax    # write() == system call 1
    mov     $1, %rdi    # stdout
    mov     $ioperm_message, %rsi
    mov     $37, %rdx   # message length
    syscall
    mov     $1, %rdi    # set retcode 1
    call     exit

start_kbd_0: 
clear_0:
  in $0x60, %al

    mov %al, %cl

loop_k_0:

#"DOES" segfault if i dont get perms...)

    /* Store the scancode to al. */
  in $0x60, %al  #segfaults immediately do i need perm?(yes its on now)
#in $0x64, %al
#in $9, %al
#in $0x16, %al

 # mov %al , %bl
  #   call exit  
#cmp %al , %cl
 #  jz loop_k_0

#❣️based on the behavior i believe this is NOT kbd output
#BUT THE INFRASTRUCTURE IS DAMN GOOD...#❣️

mov %al , (%r8,%r9,1)  #on. but going 2 fast...(only gets "�")
    # mov $49 , (%r8,%r9,1) #on
     
     #mov $91 , %rbx
  #  mov %al , %bl
   # call exit 
     
    inc %r9
  cmp $3, %r9

 
  je end_kbd2ascii_0
    jmp loop_k_0
    
    loop_clk_0:
 mov $0x08, %al  #8=month #segfaults if no perm = <3
 
 #.equ RTCaddress, 0x70
#equ RTCdata, 0x71
    out %al, $0x70
    in $0x71, %al
    
    mov     %rax, %rbx
    call     exit
    
    inc %r9
  cmp $22, %r9

  je end_kbd2ascii_0
    jmp loop_clk_0



end_kbd2ascii_0: 
 popq  %rbp
    ret 

#⌨️⌨️⌨️⌨️⌨️⌨️⌨️⌨️⌨️⌨️
#⌨️⌨️⌨️⌨️⌨️⌨️⌨️⌨️⌨️⌨️
    
    
    
    
    
    
    
    
    
    
    
    
    
    #❎️#❎️#❎️#❎️#❎️#❎️#❎️#❎️#❎️#❎️#❎️
 

#🥼️🥼️
#🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️

 
#____________________________________⚔️🩸️🩸️🩸️🩸️🩸️🩸️🩸️🩸️🩸️
.globl token_logic_2
token_logic_2:
 pushq %rbp 
    movq  %rsp, %rbp
    #_________________________
 start_token_logic_2:  
#################################
  mov $BUFFER_DATA, %r8
    mov $BUFFER_SIZE, %r9
    ####################################

   			#or swap sing 4 64 out and see what happens < THIS 	
   	 
   	 
   ########
mov $0 , %r10 # vram-stack 
mov $0 , %r11 # ram-stack count

movq $ram_64_v_0, %r12 #header goes first , ez
   	 movq $ram_64_0, %r13
   	 
   	  movq $ram_64_t_0, %r14   #will worry about names later. duh
   	 mov $0, %r15 # t-stack , count
   	 

#i may need %r11 freed up , y isn't it? 
 
 ############################################
 

 #➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️➿️💍️
 ################
     movq  $0, %rdi  #inc
    movq  $0, %rsi  #prog si.ze #maybe 4 buffer...
  
   push_program_tokens_2: 
   mov (%r8,%rsi,1) ,%cl 
 						 # sub $'0' , %rax
   push %rcx
   inc %rdi
   inc %rsi
   
   cmp $4 , %rdi     #leaving this, gonna count buffer length at the end and loop back ;)
   		#actually no, unless ur cutting stuff out of buffer.
   		
   jne push_program_tokens_2
   
   movq  $0, %rdi
    
   #tokens maybe in reverse order now... ➰️
   
    program_logic_2: 
  #######################
  #🪄️

					#🗡️ new order
mov 24(%rsp) , %rdx 		#was 'a'		#🗡️ header (ignore)							
	
mov 16(%rsp),    %rcx 	#was 'b'##🗡️ instruction 
mov 8(%rsp)  ,  %rax  # wasinstruction   #🗡️ input _ a  #obviosly u see i changes rx. makes sense tho
mov (%rsp) , %rbx #output				##🗡️ input _ b 

#superficial pops 2 stop segfault (just 4 ease out logic out <3)
pop -8(%rsp)
pop -8(%rsp)
pop -8(%rsp)
pop -8(%rsp)


#

#_____________________________replaced witch checking ALLascii
#this could probably use %rdx / rax btw  (was rbx)
 cmp  $1, %rdx #0 #ALT
    jl   end_token_logic_2# push_program_tokens_2 #next_byte
    cmp  $126 , %rdx #127 #$LOWERCASE_Z,
    jg   end_token_logic_2# push_program_tokens_2 
    
    cmp  $1, %rcx #0 #ALT
    jl   end_token_logic_2# push_program_tokens_2 #next_byte
    cmp  $126 , %rdx #127 #$LOWERCASE_Z,
    jg   end_token_logic_2# push_program_tokens_2 
    
     cmp  $1, %rax #0 #ALT
    jl   end_token_logic_2# push_program_tokens_2 #next_byte
    cmp  $126 , %rax #127 #$LOWERCASE_Z,
    jg   end_token_logic_2# push_program_tokens_2 
    
     cmp  $1, %rbx #0 #ALT
    jl   end_token_logic_2# push_program_tokens_2 #next_byte
    cmp  $126 , %rbx #127 #$LOWERCASE_Z,
    jg   end_token_logic_2# push_program_tokens_2 
    ###########this works perfectly (ITS NOT THE PROBLEM)


######
valid_entry:


#⚡️⚡️⚡️⚡️⚡️

caps_header_2:

cmp  $'A', %rdx
    jl    end_caps_header_2  # or a to b in this case 
    cmp  $'G', %rdx     #as if 4 regs (or less m8)
    jg    end_caps_header_2
    
    
 mov %rdx , (%r14,%r15,1) 	
	inc %r15
end_caps_header_2:
symbols_header_2:

end_symbols_header_2:
#⚡️⚡️⚡️⚡️⚡️

#🔌️#🔌️#🔌️#🔌️#🔌️#🔌️#🔌️#🔌️#🔌️#🔌️#🔌️#🔌️#🔌️#🔌️#🔌️



#jmp ram_drop_thru_2   #emergency skip all
#🎧️🎧️🎧️🎧️🎧️🎧️🎧️🎧️🎧️🎧️🎧️🎧️🎧️
 # so dont go out of abc order...
#🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️🪱️
wire_header_ab_2:
wire_cmp_a_2:
    cmp  $'a', %rax
    jl    wire_cmp_b_2  # or a to b in this case 
    cmp  $'z', %rax     #as if 4 regs (or less m8)
    jg    wire_cmp_b_2

    mov %rax , %r10 
     sub $97, %r10  # 48 + 50

    
   mov (%r12,%r10,1) , %al
   
   # movb $'.' , (%r12,%r10,1) 
   cmp $'.' , %rax 
   jne wire_cmp_b_2
   mov $'_' , %rax

      wire_cmp_b_2:
      
       cmp  $'a', %rbx
    jl    end_wire_header_ab_2  # or a to b in this case 
    cmp  $'z', %rbx     #as if 4 regs (or less m8)
    jg    end_wire_header_ab_2

    mov %rbx , %r10 
     sub $97, %r10  # 48 + 50

    
   mov (%r12,%r10,1) , %bl
      
    #   movb $'.' , (%r12,%r10,1) 
   cmp $'.' , %rbx 
   jne end_wire_header_ab_2
   mov $'_' , %rbx
    
    end_wire_header_ab_2:
  

 
 #✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️
###############
##👾️👾️👾️👾️👾️👾️👾️👾️👾️👾️👾️👾️👾️

##👾️👾️👾️👾️👾️👾️👾️👾️👾️👾️👾️👾️👾️
   ######################
   mult_yr_2_a:  


   finished_2:
   #📮️---------starts
inc %r11
#📮️(incs at bottom postal post;)

   #🤹‍♀️️🤹‍♀️️🤹‍♀️️🤹‍♀️️🤹‍♀️️🤹‍♀️️🤹‍♀️️🤹‍♀️️🤹‍♀️️🤹‍♀️️🤹‍♀️️

#######################################
   logic_switch_2:
   ###u may actually "NOW" move into %14/15 
   #(from "RAM" (AND IF NOTHING IS THERE BREAK OUT
#THEN U CAN REPLACE THE VALUES AFTER 
#OR JUST DELETE THEM WHEN U PULL THEM , THEN NOTHING 2 REPLACE
#HOW 2 GET VALS? = %R11 & R11 - 1 

#📍️ #empty drop with logic doesn't do anythign < BIG PROBLEM!
#why? im gonna fix that now w/e ur talking about by skipping if both r mt
#they would have dropped down by now
#we haven't made logic for "RAW NOT" so... if either is empty u need 2 
#disengage. 

ab_valid_vars_check_2: #if u add not u may change this...DONT lol..
##############################
a_valid_var_check_2:
cmp $'_' ,%rax
je end_logic_2
b_valid_var_check_2:
cmp $'_' ,%rbx 
je end_logic_2
#########################
   
   
     cmp $'_' ,%rcx  
    je end_logic_2
  
 cmp $'@' ,%rcx   
   je logical_nunq_2 # logical_nand_2
  
   #@@@@@@@@@@@@@@@@@@@@@
   logical_nunq_2: 
   #👨🏽‍🚀️
   #new logical nand (just make using "cmp $0 & $1 )
   #(cuz the arch is doing somethingweird(will try %al...
    #👨🏽‍🚀️
    sub $'0' , %rax   
  sub $'0' , %rbx   
  
  unq_fx_2: 
  
  
 cmp_a0b0:
 cmp $0 , %al
 jne cmp_a0b1
 cmp $0 , %bl
 jne cmp_a0b1
 jmp nun_fx_2
 #done cmp_a0b0 =0
 
 cmp_a0b1:

  cmp $0 , %al
 jne cmp_a1b0
 cmp $1 , %bl
 jne cmp_a1b0
 jmp nun_fx_2

 #done cmp_a0b1 = 0
 
 cmp_a1b0:
 
   cmp $1 , %al
 jne cmp_a1b1
 cmp $0 , %bl
 jne cmp_a1b1
 
 mov %bl , %al
 jmp nun_fx_2

 #done cmp_a1b0 = 0
 
 cmp_a1b1:
 #al is 1 , ur done already

 #done cmp_a1b1 = 1
  
   nun_fx_2: 
     xor $1 , %al
 add $'0' , %al 
  add $'0' , %bl
  
 
   end_logical_nunq_2: 
   mov $'_', %rbx
   jmp end_logic_2
     #👨🏽‍🚀️
   #🔥️  #also they die and burn in hell soon 4 sending me 2 shower for no reason <DRU ^^^^^^^^^^^^^^^^^
   
#https://kobzol.github.io/davis/
#riscv
#wanted to see if they booth do that (simulated)
     
     #&
#|   a   |   b   |  out  |
#|   0   |   0   |   0   |
#|   0   |   1   |   0   |
#|   1   |   0   |   0   |
#|   1   |   1   |   1   |
     
logical_nand_2:

 
# how the fuck did we get this far with literal nand
#not working when a&b swaps sides. wussup ? why? 
  sub $'0' , %rax   
  sub $'0' , %rbx   
  
    
  #proves this shit is jacked for some reason , skip it. yuk
    AND  %al  , %bl
    #AND  %bl  , %al
    
     mov %al, %bl #
  call exit #
  
    xor $1 , %al
 add $'0' , %al 
  add $'0' , %bl
  
 
  
   mov $'_', %rbx


end_logic_2:
#🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️

#🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️🥈️

################
#🪱️##🪱️##🪱️#
#do we drop alpha 2 binary? (probably not
#qo us 'up-wr' in place? no not unless header is on same wire <3 
scoop_wr_d_2: 
cmp_d:     #🧊️ (freezing this.
cmp $'_', %rdx
je end_scoop_wr_d_2

#else cmp lettering #a/b will already be in bin form so w/o u good


match_head_tail_dab_2:  
		
		
match_head_tail_d_2:	
	
	cmp  $'a', %rdx
    jl    end_scoop_wr_d_2  # or a to b in this case 
    cmp  $'z', %rdx     #as if 4 regs (or less m8)
    jg    end_scoop_wr_d_2
    
   
  
 match_head_tail_a_2: 
  cmp_a_blank_2:
 cmp  $'_', %rax
 jne pre_rdx_2
 
 cmp_b_blank_2:
 cmp  $'_', %rbx
 jne pre_rdx_2
 
 mov $'_' , %rdx  # deleting bad wiring w/e 
 
 jmp end_scoop_wr_d_2
   #both blank, should we drop wire? 
  #no thats tacky honestly u probably shouldn't drop #'s
# numdrop is ok. 4 reasons
 #i may drop args/*/w/e onto ram3 tho later.
 #(if they are on header? (or w/e if u need 4 w/e)
 #not just frivilously
 #i may use "hil" for "ram3 wiring 2 and just match"

 #🐴️
 pre_rdx_2: 
 
 mov %rdx , %r10 
 sub $97, %r10  # 48 + 50
 
 stor_rax_wr_2: 
 cmp $'_' , %rax 
 je stor_rbx_wr_2
 mov %al , (%r12,%r10,1) 
 mov $'_' , %rax
 jmp end_scoop_wr_d_2
 stor_rbx_wr_2: 
   cmp $'_' , %rbx 
 je end_scoop_wr_d_2
 mov %bl , (%r12,%r10,1) 
 mov $'_' , %rbx
     #vom 
     
      
 
end_scoop_wr_d_2:
#🪱️##🪱️##🪱️#

#✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️✂️
#################################
patch_drop_ab_2:

dec %r11



end_patch_drop_ab_2:

post_logic_check_ab_2:  
	post_logic_check_a_2:	
	
 cmp  $'_', %rax
  je post_logic_check_b_2
mov %al, (%r13,%r11,1) 
mov $'_', %rax
	#move this back in now, and add1 4 15
  inc %r11
  post_logic_check_b_2:
  cmp  $'_', %rbx
  je end_post_logic_check_ab_2
  mov %bl, (%r13,%r11,1) 
 inc %r11

#🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️
end_post_logic_check_ab_2:


#🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️🥢️
end_post_logic_check_ab_2:


#✂️✂️✂️✂️✂️✂️#🪱️
		loop_if_2:
         #🎖️
        cmp %rsi, %r9 # count vs buffsize
   
       jne push_program_tokens_2
   

   

     #____________________________
    end_token_logic_2:
 popq  %rbp
    ret 
#_________________________⚔️
#☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️

#🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️🚿️



#🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️🥼️
#🔢️🔢️🔢️🔢️
#(if u wanted 2 fix it u would just fix the 
#(buffer out from looking "crazy" 
#(dont sweat it tho maybe on bootstrap we can revist it afresh)
#🔢️🔢️🔢️🔢️
.globl buffer_stripper_0
buffer_stripper_0:
 pushq %rbp 
    movq  %rsp, %rbp
    
    mov $BUFFER_DATA, %rax
    mov $BUFFER_SIZE, %rbx
    
    
  mov $0, %rdi
    mov $0, %rsi
    cmp $0, %rbx
    je end_buffer_stripper_0
    
 strip_loop:  	# was convert_loop:
    		#will move to external file and do 'nand' 
    		#or do them out of the not gate as tx 
    mov (%rax,%rdi,1), %cl  # lower byte of rcx
    
    
  # need to jump before doing this if byte is not "1 or zero" (its editing "[,]"


cmpb $9, %cl #ascii for 'tab\ '
je next_bit_strip

cmpb $10, %cl #ascii for 'n\ '
je next_bit_strip
cmpb $32, %cl #ascii for ' '
je next_bit_strip
cmpb $44, %cl #ascii for ','
je next_bit_strip
cmpb $91, %cl #ascii for '['
je next_bit_strip
cmpb $93, %cl #ascii for '['
je next_bit_strip



  ###########################################
# think im gonna put comment stripper in new fx. 
#this one getting rediculous , its better this way trust me <3 
 to_buff_re: 
  
   mov %cl, (%rax,%rsi,1)
inc %rsi
  cmp %rdi, %rbx
    je  end_buffer_stripper_0

    next_bit_strip:    
  	inc %rdi 
    cmp %rdi, %rbx
    jne  strip_loop
    
   ##########################

  mov $0, %rdi
    mov $0, %rsi
    mov $0, %rax
    mov $0, %rbx
    mov $0, %rcx
# _________________________
end_buffer_stripper_0:  
popq  %rbp
    ret 
    
    

#🚂️🚂️🚂️🚂️🚂️🚂️🚂️

.globl buffer_comment_0
buffer_comment_0:
 pushq %rbp 
    movq  %rsp, %rbp
    
    mov $BUFFER_DATA, %rax
    mov $BUFFER_SIZE, %rbx
    
    
  mov $0, %rdi
    mov $0, %rsi
    cmp $0, %rbx
    je end_buffer_comment_0
    
 comment_loop:  	# was convert_loop:
    		#will move to external file and do 'nand' 
    		#or do them out of the not gate as tx 
    mov (%rax,%rdi,1), %cl  # lower byte of rcx


  ##########################
     cmpb $'#', %cl 
jne to_buff_com
comment_out_strip:
 mov (%rax,%rdi,1), %cl #❣️leave this, look at it, duh (confused me earlier)
 inc %rdi
 cmpb $10, %cl #ascii for 'n\'
 jne comment_out_strip

cmp %rdi, %rbx
    jne  comment_loop
   # jmp end_buffer_comment_0
  #################

 to_buff_com: 
  
   mov %cl, (%rax,%rsi,1)
inc %rsi


    next_bit_comment:    
  	inc %rdi 
    cmp %rdi, %rbx
    jne  comment_loop
    
   #################
  mov $0, %rdi
    mov $0, %rsi
    mov $0, %rax
    mov $0, %rbx
    mov $0, %rcx
 
# _________________________
end_buffer_comment_0:  
popq  %rbp
    ret 



#🚂️🚂️🚂️🚂️🚂️🚂️🚂️
##🚂️🚂️🚂️🚂️🚂️🚂️redoing "nqn" for taking in buffer and looping thr
##🚂️🚂️🚂️🚂️🚂️🚂️🚂️🚂️🚂️🚂️🚂️🚂️🚂️🚂️ make this one easy 2 understand pls.
##🚂️🚂️🚂️🚂️stripping it should be seperate fx. then u can still do what u need after
##🚂️🚂️🚂️🚂️🚂️put many tests in.
.globl buffer_engine_0 
buffer_engine_0: 
 #_________________________
end_buffer_engine_0:  
#🚂️🚂️🚂️🚂️🚂️🚂️🚂️🚂️🚂️🚂️🚂️🚂️🚂️🚂️







#xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
#xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
#xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
#xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
#xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
















#🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️🪀️




 

#☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️
#☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️
#☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️
#☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️
#☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️
#☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️
#☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️
#☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️☪️


#_________________________⚔️
#☪️




#🪳️#🪳️#🪳️#🪳️#🪳️#🪳️#🪳️#🪳️

#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️
#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️
#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️
#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️#🍀️



#🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️
#🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️
#🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️
#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️
#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️
#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️#

#🎚️#
.globl print_err_0  
print_err_0 :
 pushq %rbp 
    movq  %rsp, %rbp

start_print_err_0 :

      
   movq  $err_msg_0, %rsi      #this buffer
movq $err_msg_0_len , %rax

   movq  %rax, %rdx             #buff 2 
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_err_msg_0 :
 popq  %rbp
    ret 
#---------------------------🎚️
#---------------------------🎚️

.globl print_buff  
print_buff :
 pushq %rbp 
    movq  %rsp, %rbp

start_print_buff :

      
   movq  $BUFFER_DATA, %rsi      #this buffer
   movq $BUFFER_SIZE, %rax #replace size?
    movq  %rax, %rdx             #buff 2 
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    syscall
    
end_print_buff :
 popq  %rbp
    ret 
#---------------------------🎚️

#---------------------------🎚️

.globl print_16
print_16 :
 pushq %rbp 
    movq  %rsp, %rbp

start_print_16 :

      
   movq  $ascii_16_test_0, %rsi      #this buffer
   	
   movq $16, %rax #replace size?

    movq  %rax, %rdx             #buff 2 
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_print_16 :
 popq  %rbp
    ret 
#---------------------------🎚️




#---------------------------🎚️

.globl ascii_4_test_out_0   
ascii_4_test_out_0 :
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_4_test_out_0:

      
   movq  $ascii_4_test_0, %rsi      #this buffer
   	
   movq $4, %rax #replace size?

    movq  %rax, %rdx             #buff 2 
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_4_test_out_0 :
 popq  %rbp
    ret 
#---------------------------🎚️


#---------------------------🎚️

.globl ascii_4_head_out   
ascii_4_head_out :
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_4_head_out :

      
   movq  $ascii_4_head, %rsi      #this buffer
   	#movq 	ascii_4_a, %rsi #💡️
   movq $4, %rax #replace size?

    movq  %rax, %rdx             #buff 2 
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_4_head_out :
 popq  %rbp
    ret 
#---------------------------🎚️
#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️

#---------------------------🎚️

.globl ascii_4_a_out  
ascii_4_a_out :
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_4_a_out :

      
   movq  $ascii_4_a, %rsi      #this buffer
   	#movq 	ascii_4_a, %rsi #💡️
   movq $4, %rax #replace size?

    movq  %rax, %rdx             #buff 2 
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_4_a_out :
 popq  %rbp
    ret 
#---------------------------🎚️
#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️
#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️
#---------------------------🎚️

.globl ascii_4_b_out   
ascii_4_b_out :
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_4_b_out :

      
   movq  $ascii_4_b, %rsi      #this buffer
   	#movq 	ascii_4_a, %rsi #💡️
   movq $4, %rax #replace size?

    movq  %rax, %rdx             #buff 2 
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_4_b_out :
 popq  %rbp
    ret 
#---------------------------🎚️
#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️

#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️
#---------------------------🎚️

.globl ascii_4_i_out   
ascii_4_i_out :
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_4_i_out :

      
   movq  $ascii_4_i, %rsi      #this buffer
   	#movq 	ascii_4_a, %rsi #💡️
   movq $4, %rax #replace size?

    movq  %rax, %rdx             #buff 2 
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_4_i_out :
 popq  %rbp
    ret 
#---------------------------🎚️

#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️



#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️
#---------------------------🎚️

.globl ascii_newline 
ascii_newline:
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_newline:
#👨🏽‍⚕️️im gonna diagnose newline by pushing random stuff 2 stack and seeing
#if it messes it up 
#or just printing "ascii's as is...
      
   movq  $newline_out, %rsi      #this buffer
   	
   movq $1, %rax #lasttime i checked \n only takes 1 char ;) 

    movq  %rax, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_newline:
 popq  %rbp
    ret 
#---------------------------🎚️
#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️

#ram_32_0
 #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️
.globl ascii_32_out 
ascii_32_out:
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_32_out:

   movq  $ram_32_0, %rsi      #this buffer
   	
   movq $32, %rax #lasttime i checked \n only takes 1 char ;) 

    movq  %rax, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_32_out:
 popq  %rbp
    ret 
    
    
    
    
    
    
    #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️
#---------------------------🎚️
 #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️
.globl ascii_64_out 
ascii_64_out:
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_64_out:
#👨🏽‍⚕️️im gonna diagnose newline by pushing random stuff 2 stack and seeing
#if it messes it up 
#or just printing "ascii's as is...
      
   movq  $ram_64_0, %rsi      #this buffer
   	
   movq $64, %rax #lasttime i checked \n only takes 1 char ;) 

    movq  %rax, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_64_out:
 popq  %rbp
    ret 
    
    
    
    
    
    
    #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️
.globl ascii_64_history_out 
ascii_64_history_out:
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_64_history_out:
#👨🏽‍⚕️️im gonna diagnose newline by pushing random stuff 2 stack and seeing
#if it messes it up 
#or just printing "ascii's as is...
      
   movq  $ram_64_history_0, %rsi      #this buffer
   	
   movq $64, %rax #lasttime i checked \n only takes 1 char ;) 

    movq  %rax, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_64_history_out:
 popq  %rbp
    ret 
    #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️ 
    
    #📺️#📺️#📺️#📺️#📺️#📺️#📺️#📺️#📺️
               #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️
.globl ascii_64_t_out 
ascii_64_t_out:
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_64_t_out:
#👨🏽‍⚕️️im gonna diagnose newline by pushing random stuff 2 stack and seeing
#if it messes it up 
#or just printing "ascii's as is...
      
   movq  $ram_64_t_0, %rsi      #this buffer
   	
   movq $64, %rax #lasttime i checked \n only takes 1 char ;) 

    movq  %rax, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_64_t_out:
 popq  %rbp
    ret 
    #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️ 
    #📺️#📺️#📺️#📺️#📺️#📺️#📺️#📺️
    
            #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️
.globl ascii_64_v_out 
ascii_64_v_out:
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_64_v_out:
#👨🏽‍⚕️️im gonna diagnose newline by pushing random stuff 2 stack and seeing
#if it messes it up 
#or just printing "ascii's as is...
      
   movq  $ram_64_v_0, %rsi      #this buffer
   	
   movq $64, %rax #lasttime i checked \n only takes 1 char ;) 

    movq  %rax, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_64_v_out:
 popq  %rbp
    ret 
    #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️ 
        #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️
.globl ascii_64_v_history_out 
ascii_64_v_history_out:
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_64_v_history_out:
#👨🏽‍⚕️️im gonna diagnose newline by pushing random stuff 2 stack and seeing
#if it messes it up 
#or just printing "ascii's as is...
      
   movq  $ram_64_v_history_0, %rsi      #this buffer
   	
   movq $64, %rax #lasttime i checked \n only takes 1 char ;) 

    movq  %rax, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_64_v_history_out:
 popq  %rbp
    ret 
    #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️ 
    
    
    
     #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️
.globl ascii_128_v_out 
ascii_128_v_out:
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_128_v_out:

      
   movq  $ram_128_v_0, %rsi      #this buffer
   	
   movq $128, %rax #lasttime i checked \n only takes 1 char ;) 

    movq  %rax, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_128_v_out:
 popq  %rbp
    ret 
    #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️ 
    
#---------------------------🎚️
#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️

    
     #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️
.globl ascii_256_v_out 
ascii_256_v_out:
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_256_v_out:

      
   movq  $ram_256_v_0, %rsi      #this buffer
   	
   movq $256, %rax #lasttime i checked \n only takes 1 char ;) 

    movq  %rax, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_256_v_out:
 popq  %rbp
    ret 
    #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️ 
    
#---------------------------🎚️
#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️
#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️
#---------------------------🎚️

#---------------------------🎚️
#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️

    
     #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️
.globl ascii_256_cli_out 
ascii_256_cli_out:
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_256_cli_out:

      
   movq  $ram_256_cli_0, %rsi      #this buffer
   	
   movq $256, %rax #lasttime i checked \n only takes 1 char ;) 

    movq  %rax, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_256_cli_out:
 popq  %rbp
    ret 
    #🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️🖨️🐏️ 
    
#---------------------------🎚️
#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️
#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️
#---------------------------🎚️
#🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️
#https://forum.nasm.us/index.php?topic=98.0 (nasm speaker syscal8)
.globl ascii_sing_out 
ascii_sing_out :
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_sing_out:
      
   movq  $ascii_sing, %rsi      #this buffer
   	
   movq $1, %rax #lasttime i checked \n only takes 1 char ;) 

    movq  %rax, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_sing_out :
 popq  %rbp
    ret 
    
    #🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️🎙️
#---------------------------🎚️
#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️
#엘 sing test. not gonna work but w/e lets test it. 

#🎙️🧪️🎙️🧪️🎙️🧪️🎙️🧪️🎙️🧪️🎙️🧪️🎙️🧪️
#https://forum.nasm.us/index.php?topic=98.0 (nasm speaker syscal8)
.globl ascii_sing_test 
ascii_sing_test :
 pushq %rbp 
    movq  %rsp, %rbp

start_ascii_sing_test:
      
   movq  $sing_test, %rsi      #this buffer
   	
   movq $1, %rax #lasttime i checked \n only takes 1 char ;) 

    movq  %rax, %rdx             #buff size
    movq  $SYS_WRITE, %rax        #write to
    movq  ST_FD_OUT(%rbp), %rdi   #to this file s
    
    syscall
    
end_ascii_sing_test :
 popq  %rbp
    ret 
#🎙️🧪️🎙️🧪️🎙️🧪️🎙️🧪️🎙️🧪️🎙️🧪️🎙️🧪️
#------------------------------------🈺️
#sys in . #can still io from ascii so no biggie
.globl sys_in
sys_in:
 pushq %rbp 
    movq  %rsp, %rbp

start_sys_in:

open_files:
open_fd_in:

#
mov ST_ARGC(%rbp) , %rbx 
#call exit 
cmpq $2, %rbx  #(use the real position if u want ...duh
jz store_fd_stdin

  mov $filename_in, %rdi # 🚩            
   
    mov $O_RDONLY, %rsi   
      # mov $O_CREAT_WRONLY_TRUNC, %rsi
      
    mov $0666, %rdx                    
    mov $SYS_OPEN, %rax 

    syscall #🏕️
store_fd_in:
    #save the given file descriptor
    movq  %rax, ST_FD_IN(%rbp)
    

  #--------------------
    jmp end_sys_in # open_fd_out
    
    store_fd_stdin:
#save the given file descriptor

mov ST_ARGV_1(%rbp), %rdi   # address of input filename
    mov $O_RDONLY, %rsi
    mov $SYS_OPEN, %rax
   

    syscall #🏕️

    #save the given file descriptor
    movq  %rax, ST_FD_IN(%rbp)

end_sys_in:
 popq  %rbp
    ret 
#---------------------------🈺️
#🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️🎚️
#--------------------------------------🎛️

.globl sys_out
sys_out:
 pushq %rbp 
    movq  %rsp, %rbp

start_sys_out:

  open_fd_out:
 
cmpq $3, ST_ARGC(%rbp)  #will auto jump unless 3 args (aren't (maynot even do that anywas lol)
jne store_fd_stdout

###OPEN OUTPUT FILE###
    movq  $SYS_OPEN, %rax          #open 
    movq  ST_ARGV_2(%rbp), %rdi    #this file
    movq  $O_CREAT_WRONLY_TRUNC, %rsi          #with write_only flag
    movq  $0666, %rdx              #(if needed creation) with mode 666
    syscall                        #file id returned in %rax
store_fd_out:
    #store the file descriptor here
    movq  %rax, ST_FD_OUT(%rbp)
 
    jmp  end_sys_out #read_loop_begin  #🀄️ 
    
    store_fd_stdout:
#store the file descriptor here
movl $STDOUT, ST_FD_OUT(%rbp)
end_sys_out:

 popq  %rbp
    ret 

#---------------------------------------🎚️
 

#i dont think u have 2 keep re putting "MODEM IN addr into var"
#but w/e 
#i do think ur gonna want to open only specific "PORTS"
#so u may be right 2 do in fx and change "mov $logname_out, %rdi #🚩 "
#depending , (may even make ur own "string_#18+.txt" for each log addr

#📤️☎️📤️☎️📤️☎️📤️☎️📤️☎️📤️☎️
.globl modem_out_open
modem_out_open:
 pushq %rbp 
    movq  %rsp, %rbp


 popq  %rbp
    ret 

#📤️☎️📤️☎️📤️☎️📤️☎️📤️☎️📤️☎️

#💾️💾️💾️💾️💾️💾️
#save_state_open
#save_state_load

#load_state_open
#load_state_load
#💾️💾️💾️💾️💾️💾️


#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️
.globl log_open
log_open:
 pushq %rbp 
    movq  %rsp, %rbp

start_log_open:
	#sameV
 mov $logname_out, %rdi #🚩   ##🛍️ 🔛address of output filename]ON
    mov $0666, %rdx                     # perms
    mov $0x441, %rsi  #tweak this (append+ (for better performance.
    #https://stackoverflow.com/questions/48541868/how-to-append-to-a-file-using-x86-64-linux-system-calls
    mov $SYS_OPEN, %rax #🚩 #💰 file addr stored in rax after syscall
    syscall #🏕️
    
    mov %rax, ST_LOG_OUT_0(%rbp)  #file addr...(not data)
    
    end_log_open:
 popq  %rbp
    ret 

#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️
.globl log_out
log_out:
 pushq %rbp 
    movq  %rsp, %rbp
    
    
       start_log_out: 

  #  mov  $newline_out , %rsi      #this buffer 
  #  movq  $1, %rdx             #buff size
  #  movq  $SYS_WRITE, %rax        #write to
  #  movq  ST_LOG_OUT_0(%rbp), %rdi   #to this file s   
  #  syscall
   
end_log_out:
 popq  %rbp
    ret 
#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️
#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️#🖨️



#🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️
#🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️
#🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️🧊️

#------------------------------------🎛️
.globl load_buff  #called continue but call from "MAIN" same as 123
load_buff:
 pushq %rbp 
    movq  %rsp, %rbp

start_load_buff:
# 🕹️
###BEGIN MAIN LOOP###
load_loop_begin_0:
    ###READ IN A BLOCK FROM THE INPUT FILE###
    movq  $SYS_READ, %rax       #read
    movq  ST_FD_IN(%rbp), %rdi  #from this file
    movq  $BUFFER_DATA, %rsi    #to this buffer
    movq  $BUFFER_SIZE, %rdx    #up to this count
    syscall                     #size of buffer read is returned in %rax
    
    ###EXIT IF WE’VE REACHED THE END###
    #check for end of file marker
    cmpq  $END_OF_FILE, %rax
    #if found or on error, go to the end
    jle   end_load_buff # end_loop
    
    continue_load_loop_0:
###CONVERT THE BLOCK TO UPPER CASE###
    movq $BUFFER_DATA, %rdi    #location of buffer #
    #jul]13 < dont think we need ^ but im doing modem dont wanna break
# 🕹️
    movq %rax, %rsi            #size of the buffer ###🕹️
 
#🥼️]🥼️]🥼️]🥼️]🥼️]🥼️]🥼️]🥼️]

end_load_buff:
 popq  %rbp
    ret 
#---------------------------🎚️


#🔏️🔏️🔏️🔏️SP.FARMING THEORY = SUPREME🔏️🔏️🔏️🔏️🔏️>the fact that this NOS STUFF DOESN"T "MATTER" 
#and that u could even "LOSE SOLPEN" w/o losing the understandings of it 
#IS A GOOD THINGP{but dont...}}
#{>pin}? i think (like ~v~) thse things are "spirituall 'from sp" but dont 'rap/deplete its core
#they actually enrich >pin and "self sustain it. for movies or w/e i think nugget drops of core 
#beats should be like breadcrums and most importantly u should always legally protect IP test waters cuz its meeting reality 
#example using clones dosen't have 2 relate  2 >pin anymore than star wars (but it DOES)
#and cloning is more inportant than ent we already have engt somewere and it can infinitely 
#spin off multiverses that can actually disapear w/o harming core....

#example i really think this "Nugget" if understood is "MOR IMPORTANT" than sp itself
#4 the survival of "SP" (legallly etc) and i can just drop it at the bottom of pages etc...
#also this using "SP as GOL SETTING , is more important than ent as well imo
# = 🌊️🌍️    🌛️🍒️🌞️🛰️🕌️🎱️🕸️🪐️⛩️⌚️🐩️🔏️~
#🔏️🔏️🔏️🔏️🔏️🔏️🔏️🔏️🔏️

 
  
