#❌️ (remove all addition)
sed -i.bak '/^\s*source\s/d' ~/.bashrc          

#(+reset "temp? PATH")
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin"
#❌️
