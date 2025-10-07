#🏣️
input=$1
inputDirective1=$4 # << or <
inputDirective2=$5 # << or <
file=${input%.*}

output_dir="+x"
button_dir="./"

# gcc "$file".c -lpthread -lGL -lGLU -lglut -lm -o +x/"$file".+x
# -o "$output_dir/$executable_name" 
 
gcc "$file".c -o $button_dir/"$file".+x -pthread -lm -lssl -lcrypto -lGL -lGLU -lglut -lfreetype -lavcodec -lavformat -lavutil -lswscale  -lX11 -I/usr/include/freetype2 -I/usr/include/libpng12 -L/usr/local/lib -lOpenCL 
 
 
 echo $?
 

