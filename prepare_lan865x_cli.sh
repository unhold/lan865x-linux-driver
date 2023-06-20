#!/bin/bash

save_configure(){
dos2unix lan865x-overlay-tmp.dts &> /dev/null
dtc -I dts -O dtb -o lan865x.dtbo lan865x-overlay-tmp.dts
search=$(find -type f -name lan865x.dtbo)
if [[ $search ]]
then
sudo cp lan865x.dtbo /boot/overlays/
file="/boot/config.txt"
found=0
while read -r line; do
	if [[ "$line" == "dtparam=spi=on" ]]
	then
		found=1
		break
	fi
done <$file
if [ $found != 1 ]
then
	echo "dtparam=spi=on" | sudo tee -a $file > /dev/null
fi
found=0
while read -r line; do
	if [[ "$line" == "dtoverlay=lan865x" ]]
	then
		found=1
		break
	fi
done <$file
if [ $found != 1 ]
then
	echo "dtoverlay=lan865x" | sudo tee -a $file > /dev/null
fi
echo ""
	cp lan865x-overlay-tmp.dts dts/lan865x-overlay.dts
	dos2unix dts/lan865x-overlay.dts &> /dev/null
	cp load-tmp.sh load.sh
	chmod +x load-tmp.sh
	echo "LAN865x configuration is successful...!"
echo ""
else
echo ""
	echo "LAN865x configuration is failed...!"
echo ""
fi
}

install_lan865x_driver(){
rm lan865x_t1s.ko &> /dev/null
make clean
make

search=$(find -type f -name lan865x_t1s.ko)
if [[ $search ]]
then
file="/etc/rc.local"

found=0
while read -r line; do
	if [[ "$line" == "#Configure lan865x driver loading" ]]
	then
		found=1
		break
	fi
done <$file
if [ $found != 1 ]
then
	sed '/^exit 0.*/i #Configure lan865x driver loading\nDRV_PATH\nsudo ./load.sh\ncd -\n' $file | sudo tee /etc/rc.local_tmp > /dev/null
	sudo mv /etc/rc.local_tmp $file
	v1=$(awk '/DRV_PATH/{ print NR; exit }' $file)
	awk -F"=" -v newval="cd $PWD" -v v2=$v1 '{ if (NR == v2) print newval ; else print $0}' $file | sudo tee /etc/rc.local_tmp > /dev/null
	sudo mv /etc/rc.local_tmp $file
	sudo chmod 755 $file
fi

echo ""

	echo "LAN865x driver compilation and installation is successful...!"
echo ""
else
echo ""
	echo "LAN865x driver compilation and installation is failed...!"
echo ""
fi

}

configure_lan865x_1(){
while [ 1 ];
do
printf "\033c"
echo "1st LAN865x PLCA Settings:"
echo "--------------------------"

v1="$(awk '{if(NR==49) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_mode_1=$v2
echo "	1. PLCA mode		: $v2"

v1="$(awk '{if(NR==50) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_node_id_1=$v2
echo "	2. PLCA node id		: $v2"

v1="$(awk '{if(NR==51) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_node_count_1=$v2
echo "	3. PLCA node count	: $v2"

v1="$(awk '{if(NR==52) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_burst_count_1=$v2
echo "	4. PLCA burst count	: $v2"

v1="$(awk '{if(NR==53) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_burst_timer_1=$v2
echo "	5. PLCA burst timer	: $v2"

v1="$(awk '{if(NR==54) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_to_timer_1=$v2
echo "	6. PLCA TO timer	: $v2"

v1="$(awk '{if(NR==55) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
tx_cut_through_1=$v2
echo "	7. Tx cut through	: $v2"

v1="$(awk '{if(NR==56) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
rx_cut_through_1=$v2
echo "	8. Rx cut through	: $v2"

v1="$(awk '{if(NR==46) print $0}' lan865x-overlay-tmp.dts)"
v2=$(grep -oP '= \K.*?(?=;)' <<< "$v1")
v2=$(echo "$v2" | sed -r 's/[ ]+/:/g')
v2=${v2:1}
v2=${v2:0:17}
mac_addr_1=$v2
echo "	9. MAC address 		: $v2"

v1="$(awk '{if(NR==8) print $0}' load-tmp.sh)"
v2=$(cut -d "=" -f2- <<< $v1)
ip_addr_1=$v2
echo "	10. IP address		: $ip_addr_1"

v1="$(awk '{if(NR==9) print $0}' load-tmp.sh)"
v2=$(cut -d "=" -f2 <<< "$v1")
subnet_mask_1=$v2
echo "	11. Subnet mask		: $subnet_mask_1"

echo "	12. Save & Configure"

echo "	99. Back
	0. Exit"
echo "Enter your option:"
read option
if [ $option == 1 ]
then
echo "Enter PLCA mode (0-disable, 1-enable):"
read plca_mode_1
if [[ $plca_mode_1 == 0 ]] || [[ $plca_mode_1 == 1 ]]
then
data="                                plca-enable = /bits/ 8 <$plca_mode_1>; /* 1 - PLCA enable, 0 - CSMA/CD enable */"
awk -F"=" -v newval="$data" '{ if (NR == 49) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
else
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
fi
elif [ $option == "2" ]
then
if [ $plca_mode_1 == 0 ]
then
echo "PLCA mode is disabled"
read -p "Press Enter key to continue...!"
else
echo "Enter PLCA node id [0-254]"
read plca_node_id_1
if [[ -n ${plca_node_id_1//[0-9]/} ]];
then
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
else
if [[ $plca_node_id_1 -lt 0 ]] || [[ $plca_node_id_1 -gt 254 ]]
then
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
else
data="                                plca-node-id = /bits/ 8 <$plca_node_id_1>; /* PLCA node id range: 0 to 254 */"
awk -F"=" -v newval="$data" '{ if (NR == 50) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
fi
fi
fi
elif [ $option == 3 ]
then
if [ $plca_mode_1 == 0 ]
then
echo "PLCA mode is disabled"
else
echo "Enter PLCA node count [2-255]"
read plca_node_count_1
if [[ -n ${plca_node_count_1//[0-9]/} ]];
then
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
else
if [[ $plca_node_count_1 -lt 2 ]] || [[ $plca_node_count_1 -gt 255 ]]
then
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
else
data="                                plca-node-count = /bits/ 8 <$plca_node_count_1>; /* PLCA node count range: 1 to 255 */"
awk -F"=" -v newval="$data" '{ if (NR == 51) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
fi
fi
fi
elif [ $option == 4 ]
then
if [ $plca_mode_1 == 0 ]
then
echo "PLCA mode is disabled"
read -p "Press Enter key to continue...!"
else
echo "Enter PLCA burst count [0x0-0xFF]"
read plca_burst_count_1
v1=${plca_burst_count_1:0:2}
if [[ $v1 == "0x" ]]
then
plca_burst_count_1=${plca_burst_count_1:2}
fi
if [[ ${#plca_burst_count_1} -lt 1 ]] || [[ ${#plca_burst_count_1} -gt 2 ]] || ! [[ $plca_burst_count_1 =~ ^[0-9A-Fa-f]{1,}$ ]]
then
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
else
data="                                plca-burst-count = /bits/ 8 <0x$plca_burst_count_1>; /* PLCA burst count range: 0x0 to 0xFF */"
awk -F"=" -v newval="$data" '{ if (NR == 52) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
fi
fi
elif [ $option == 5 ]
then
if [ $plca_mode_1 == 0 ]
then
echo "PLCA mode is disabled"
read -p "Press Enter key to continue...!"
else
echo "Enter PLCA burst timer [0x0-0xFF]"
read plca_burst_timer_1
v1=${plca_burst_timer_1:0:2}
if [[ $v1 == "0x" ]]
then
plca_burst_timer_1=${plca_burst_timer_1:2}
fi
if [[ ${#plca_burst_timer_1} -lt 1 ]] || [[ ${#plca_burst_timer_1} -gt 2 ]] || ! [[ $plca_burst_timer_1 =~ ^[0-9A-Fa-f]{1,}$ ]]
then
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
else
data="                                plca-burst-timer = /bits/ 8 <0x$plca_burst_timer_1>; /* PLCA burst timer */"
awk -F"=" -v newval="$data" '{ if (NR == 53) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
fi
fi
elif [ $option == 6 ]
then
if [ $plca_mode_1 == 0 ]
then
echo "PLCA mode is disabled"
read -p "Press Enter key to continue...!"
else
echo "Enter PLCA TO timer [0x0-0xFF]"
read plca_to_timer_1
v1=${plca_to_timer_1:0:2}
if [[ $v1 == "0x" ]]
then
plca_to_timer_1=${plca_to_timer_1:2}
fi
if [[ ${#plca_to_timer_1} -lt 1 ]] || [[ ${#plca_to_timer_1} -gt 2 ]] || ! [[ $plca_to_timer_1 =~ ^[0-9A-Fa-f]{1,}$ ]]
then
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
else
data="                                plca-to-timer = /bits/ 8 <0x$plca_to_timer_1>; /* PLCA TO timer */"
awk -F"=" -v newval="$data" '{ if (NR == 54) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
fi
fi
elif [ $option == 7 ]
then
echo "Enter Tx cut through mode [0-disable, 1-enable]"
read tx_cut_through_1
if [[ $tx_cut_through_1 == 0 ]] || [[ $tx_cut_through_1 == 1 ]]
then
data="                                tx-cut-through-mode = /bits/ 8 <$tx_cut_through_1>; /* 1 - Tx cut through mode enable, 0 - Store and forward mode enable */"
awk -F"=" -v newval="$data" '{ if (NR == 55) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
else
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
fi
elif [ $option == 8 ]
then
echo "Enter Rx cut through mode [0-disable, 1-enable]"
read rx_cut_through_1
if [[ $rx_cut_through_1 == 0 ]] || [[ $rx_cut_through_1 == 1 ]]
then
data="                                rx-cut-through-mode = /bits/ 8 <$rx_cut_through_1>; /* 1 - Rx cut through mode enable, 0 - Store and forward mode enable */"
awk -F"=" -v newval="$data" '{ if (NR == 56) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
else
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
fi
elif [ $option == 9 ]
then
echo "Enter the MAC address:"
read mac_addr_1
mac_addr_1_tmp=$mac_addr_1
if [[ $mac_addr_1 =~ ^([[:xdigit:]]{2}:){5}[[:xdigit:]]{2}$ ]]; then
mac_addr_1=$(echo "$mac_addr_1" | sed -r 's/[:]+/ /g')
data="                                local-mac-address = [$mac_addr_1];"
awk -F"=" -v newval="$data" '{ if (NR == 46) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
data="mac_addr_1=$mac_addr_1_tmp"
awk -F"=" -v newval="$data" '{ if (NR == 7) print newval ; else print $0}' load-tmp.sh > load-tmp_tmp.sh
mv load-tmp_tmp.sh load-tmp.sh
chmod +x load-tmp.sh
else
echo "Invalid MAC address...!"
read -p "Press Enter key to continue...!"
fi
elif [ $option == 10 ]
then
echo "Enter 1st LAN865x (eth1) IP address:"
read input
if [[ "$input" =~ ^(([1-9]?[0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))\.){3}([1-9]?[0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))$ ]]; then
ip_addr_1=$input
data="ip_addr_1=$ip_addr_1"
awk -F"=" -v newval="$data" '{ if (NR == 8) print newval ; else print $0}' load-tmp.sh > load-tmp_tmp.sh
mv load-tmp_tmp.sh load-tmp.sh
chmod +x load-tmp.sh
else
echo "Invalid IP...!"
read -p "Press Enter key to continue...!"
fi

elif [ $option == 11 ]
then
echo "Enter 1st LAN865x (eth1) Subnet mask:"
read input
if [[ "$input" =~ ^(([1-9]?[0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))\.){3}([1-9]?[0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))$ ]]; then
subnet_mask_1=$input
data="subnet_mask_1=$subnet_mask_1"
awk -F"=" -v newval="$data" '{ if (NR == 9) print newval ; else print $0}' load-tmp.sh > load-tmp_tmp.sh
mv load-tmp_tmp.sh load-tmp.sh
chmod +x load-tmp.sh
else
echo "Invalid Subnet mask...!"
read -p "Press Enter key to continue...!"
fi

elif [ $option == 12 ]
then
	save_configure
	read -p "Press Enter key to continue...!"
elif [ $option == 99 ]
then
	break
elif [ $option == 0 ]
then
	rm lan865x-overlay-tmp.dts
	exit 0
else
	echo "Invalid input...!"
	read -p "Press Enter key to continue...!"
fi
done
}

configure_lan865x_2(){
while [ 1 ];
do
printf "\033c"
echo "2nd LAN865x PLCA Settings:"
echo "--------------------------"

v1="$(awk '{if(NR==28) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_mode_2=$v2
echo "	1. PLCA mode		: $v2"

v1="$(awk '{if(NR==29) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_node_id_2=$v2
echo "	2. PLCA node id		: $v2"

v1="$(awk '{if(NR==30) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_node_count_2=$v2
echo "	3. PLCA node count	: $v2"

v1="$(awk '{if(NR==31) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_burst_count_2=$v2
echo "	4. PLCA burst count	: $v2"

v1="$(awk '{if(NR==32) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_burst_timer_2=$v2
echo "	5. PLCA burst timer	: $v2"

v1="$(awk '{if(NR==33) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_to_timer_2=$v2
echo "	6. PLCA to timer	: $v2"

v1="$(awk '{if(NR==34) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
tx_cut_through_2=$v2
echo "	7. Tx cut through	: $v2"

v1="$(awk '{if(NR==35) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
rx_cut_through_2=$v2
echo "	8. Rx cut through	: $v2"

v1="$(awk '{if(NR==25) print $0}' lan865x-overlay-tmp.dts)"
v2=$(grep -oP '= \K.*?(?=;)' <<< "$v1")
v2=$(echo "$v2" | sed -r 's/[ ]+/:/g')
v2=${v2:1}
v2=${v2:0:17}
echo "	9. MAC address		: $v2"

v1="$(awk '{if(NR==12) print $0}' load-tmp.sh)"
v2=$(cut -d "=" -f2- <<< $v1)
ip_addr_2=$v2
echo "	10. IP address		: $ip_addr_2"

v1="$(awk '{if(NR==13) print $0}' load-tmp.sh)"
v2=$(cut -d "=" -f2 <<< "$v1")
subnet_mask_2=$v2
echo "	11. Subnet mask		: $subnet_mask_2"

echo "	12. Save & Configure"

echo "	99. Back
	0. Exit"
echo "Enter your option:"
read option
if [ $option == 1 ]
then
echo "Enter PLCA mode (0-disable, 1-enable):"
read plca_mode_2
if [[ $plca_mode_2 == 0 ]] || [[ $plca_mode_2 == 1 ]]
then
data="                                plca-enable = /bits/ 8 <$plca_mode_2>; /* 1 - PLCA enable, 0 - CSMA/CD enable */"
awk -F"=" -v newval="$data" '{ if (NR == 28) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
else
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
fi
elif [ $option == 2 ]
then
if [ $plca_mode_2 == 0 ]
then
echo "PLCA mode is disabled...!"
read -p "Press Enter key to continue...!"
else
echo "Enter PLCA node id [0-254]"
read plca_node_id_2
if [[ -n ${plca_node_id_2//[0-9]/} ]];
then
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
else
if [[ $plca_node_id_2 -lt 0 ]] || [[ $plca_node_id_2 -gt 254 ]]
then
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
else
data="                                plca-node-id = /bits/ 8 <$plca_node_id_2>; /* PLCA node id range: 0 to 254 */"
awk -F"=" -v newval="$data" '{ if (NR == 29) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
fi
fi
fi
elif [ $option == 3 ]
then
if [ $plca_mode_2 == 0 ]
then
echo "PLCA mode is disabled...!"
read -p "Press Enter key to continue...!"
else
echo "Enter PLCA node count [2-255]"
read plca_node_count_2
if [[ -n ${plca_node_count_2//[0-9]/} ]];
then
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
else
if [[ $plca_node_count_2 -lt 2 ]] || [[ $plca_node_count_2 -gt 255 ]]
then
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
else
data="                                plca-node-count = /bits/ 8 <$plca_node_count_2>; /* PLCA node count range: 1 to 255 */"
awk -F"=" -v newval="$data" '{ if (NR == 30) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
fi
fi
fi
elif [ $option == 4 ]
then
if [ $plca_mode_2 == 0 ]
then
echo "PLCA mode is disabled...!"
read -p "Press Enter key to continue...!"
else
echo "Enter PLCA burst count [0x0-0xFF]"
read plca_burst_count_2
v1=${plca_burst_count_2:0:2}
if [[ $v1 == "0x" ]]
then
plca_burst_count_2=${plca_burst_count_2:2}
fi
if [[ ${#plca_burst_count_2} -lt 1 ]] || [[ ${#plca_burst_count_2} -gt 2 ]] || ! [[ $plca_burst_count_2 =~ ^[0-9A-Fa-f]{1,}$ ]]
then
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
else
data="                                plca-burst-count = /bits/ 8 <0x$plca_burst_count_2>; /* PLCA burst count range: 0x0 to 0xFF */"
awk -F"=" -v newval="$data" '{ if (NR == 31) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
fi
fi
elif [ $option == 5 ]
then
if [ $plca_mode_2 == 0 ]
then
echo "PLCA mode is disabled...!"
read -p "Press Enter key to continue...!"
else
echo "Enter PLCA burst timer [0x0-0xFF]"
read plca_burst_timer_2
v1=${plca_burst_timer_2:0:2}
if [[ $v1 == "0x" ]]
then
plca_burst_timer_2=${plca_burst_timer_2:2}
fi
if [[ ${#plca_burst_timer_2} -lt 1 ]] || [[ ${#plca_burst_timer_2} -gt 2 ]] || ! [[ $plca_burst_timer_2 =~ ^[0-9A-Fa-f]{1,}$ ]]
then
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
else
data="                                plca-burst-timer = /bits/ 8 <0x$plca_burst_timer_2>; /* PLCA burst timer */"
awk -F"=" -v newval="$data" '{ if (NR == 32) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
fi
fi
elif [ $option == 6 ]
then
if [ $plca_mode_2 == 0 ]
then
echo "PLCA mode is disabled...!"
read -p "Press Enter key to continue...!"
else
echo "Enter PLCA TO timer [0x0-0xFF]"
read plca_to_timer_2
v1=${plca_to_timer_2:0:2}
if [[ $v1 == "0x" ]]
then
plca_to_timer_2=${plca_to_timer_2:2}
fi
if [[ ${#plca_to_timer_2} -lt 1 ]] || [[ ${#plca_to_timer_2} -gt 2 ]] || ! [[ $plca_to_timer_2 =~ ^[0-9A-Fa-f]{1,}$ ]]
then
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
else
data="                                plca-to-timer = /bits/ 8 <0x$plca_to_timer_2>; /* PLCA TO timer */"
awk -F"=" -v newval="$data" '{ if (NR == 33) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
fi
fi
elif [ $option == 7 ]
then
echo "Enter Tx cut through mode [0-disable, 1-enable]"
read tx_cut_through_2
if [[ $tx_cut_through_2 == 0 ]] || [[ $tx_cut_through_2 == 1 ]]
then
data="                                tx-cut-through-mode = /bits/ 8 <$tx_cut_through_2>; /* 1 - Tx cut through mode enable, 0 - Store and forward mode enable */"
awk -F"=" -v newval="$data" '{ if (NR == 34) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
else
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
fi
elif [ $option == 8 ]
then
echo "Enter Rx cut through mode [0-disable, 1-enable]"
read rx_cut_through_2
if [[ $rx_cut_through_2 == 0 ]] || [[ $rx_cut_through_2 == 1 ]]
then
data="                                rx-cut-through-mode = /bits/ 8 <$rx_cut_through_2>; /* 1 - Rx cut through mode enable, 0 - Store and forward mode enable */"
awk -F"=" -v newval="$data" '{ if (NR == 35) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
else
echo "Invalid input...!"
read -p "Press Enter key to continue...!"
fi
elif [ $option == 9 ]
then
echo "Enter the MAC address:"
read mac_addr_2
mac_addr_2_tmp=$mac_addr_2
if [[ $mac_addr_2 =~ ^([[:xdigit:]]{2}:){5}[[:xdigit:]]{2}$ ]]; then
mac_addr_2=$(echo "$mac_addr_2" | sed -r 's/[:]+/ /g')
data="                                local-mac-address = [$mac_addr_2];"
awk -F"=" -v newval="$data" '{ if (NR == 25) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
data="mac_addr_2=$mac_addr_2_tmp"
awk -F"=" -v newval="$data" '{ if (NR == 11) print newval ; else print $0}' load-tmp.sh > load-tmp_tmp.sh
mv load-tmp_tmp.sh load-tmp.sh
chmod +x load-tmp.sh
else
echo "Invalid MAC address...!"
read -p "Press Enter key to continue...!"
fi
elif [ $option == 10 ]
then
echo "Enter 2nd LAN865x (eth2) IP address:"
read input
if [[ "$input" =~ ^(([1-9]?[0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))\.){3}([1-9]?[0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))$ ]]; then
ip_addr_2=$input
data="ip_addr_2=$ip_addr_2"
awk -F"=" -v newval="$data" '{ if (NR == 12) print newval ; else print $0}' load-tmp.sh > load-tmp_tmp.sh
mv load-tmp_tmp.sh load-tmp.sh
chmod +x load-tmp.sh
else
echo "Invalid IP...!"
read -p "Press Enter key to continue...!"
fi

elif [ $option == 11 ]
then
echo "Enter 2nd LAN865x (eth2) Subnet mask:"
read input
if [[ "$input" =~ ^(([1-9]?[0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))\.){3}([1-9]?[0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))$ ]]; then
subnet_mask_2=$input
data="subnet_mask_2=$subnet_mask_2"
awk -F"=" -v newval="$data" '{ if (NR == 13) print newval ; else print $0}' load-tmp.sh > load-tmp_tmp.sh
mv load-tmp_tmp.sh load-tmp.sh
chmod +x load-tmp.sh
else
echo "Invalid Subnet mask...!"
read -p "Press Enter key to continue...!"
fi

elif [ $option == 12 ]
then
	save_configure
	read -p "Press Enter key to continue...!"
elif [ $option == 99 ]
then
	break
elif [ $option == 0 ]
then
	rm lan865x-overlay-tmp.dts
	exit 0
else
	echo "Invalid input...!"
	read -p "Press Enter key to continue...!"
fi
done
}

dpkg -s "device-tree-compiler" &> /dev/null
if [ $? != 0 ]
then
echo "Package device-tree-compiler is not installed...!"
echo "Do you want to install it ?"
echo " 	1. Yes
	0. No"
echo "Enter your option:"
read option
if [ $option == 1 ]
then
	sudo apt-get install device-tree-compiler
dpkg -s "device-tree-compiler" &> /dev/null
if [ $? != 0 ]
then
echo "Package device-tree-compiler is not installed...!"
echo "Please install device-tree-compiler and try again...!"
exit 0
else
echo ""
echo "Package device-tree-compiler installed successfully...!"
echo ""
read -p "Press Enter key to continue...!"
fi
elif [ $option == 0 ]
then
	echo "Please install device-tree-compiler and try again...!"
	exit 0
else
	echo "Invalid input...!"
	exit 0
fi
fi
search=$(find /lib/modules/$(uname -r)/ -type l -name "build")
if [[ $search ]]
then
echo ""
else
echo "Kernel headers are not installed...!"
echo "Do you want to install it ?"
echo " 	1. Yes
	0. No"
echo "Enter your option:"
read option
if [ $option == 1 ]
then
echo "Kernel headers installation started and it will take some minutes...!"
	sudo apt-get --assume-yes install build-essential cmake subversion libncurses5-dev bc bison flex libssl-dev python2
	sudo wget https://raw.githubusercontent.com/RPi-Distro/rpi-source/master/rpi-source -O /usr/local/bin/rpi-source && sudo chmod +x /usr/local/bin/rpi-source && /usr/local/bin/rpi-source -q --tag-update
	rpi-source  --skip-gcc
	search=$(find /lib/modules/$(uname -r)/ -type l -name "build")
if [[ $search ]]
then
	echo ""
	echo "Kernel headers installation is successful...!"
	echo ""
	read -p "Press Enter key to continue...!"
else
	echo ""
	echo "Kernel headers installation is failed...!"
	echo ""
	exit 0
fi
elif [ $option == 0 ]
then
	echo "Please install kernel headers and try again...!"
	exit 0
else
	echo "Invalid input...!"
	exit 0
fi
fi
while [ 1 ];
do
cp dts/lan865x-overlay.dts lan865x-overlay-tmp.dts
cp load.sh load-tmp.sh
printf "\033c"
echo "		This script is for configuring LAN865x in Raspberry Pi"
echo "		--------------------------------------------------------"
echo "LAN865x Configuration Options:"
echo "------------------------------"
echo "	1. Configure 1st LAN865x (eth1)
	2. Configure 2nd LAN865x (eth2)
	3. Install Driver
	0. Exit"
echo "Enter your option:"
read option
if [ $option == 1 ]
then
	configure_lan865x_1
elif [ $option == 2 ]
then
	configure_lan865x_2
elif [ $option == 3 ]
then
	install_lan865x_driver
	read -p "Press Enter key to continue...!"
elif [ $option == 0 ]
then
	rm lan865x-overlay-tmp.dts
	rm load-tmp.sh
	exit 0
else
	echo "Invalid input...!"
	read -p "Press Enter key to continue...!"
fi
done
