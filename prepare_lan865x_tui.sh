#!/bin/bash

install_driver(){
clear
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
	dialog --title "Success" --clear "$@" --msgbox "LAN865x Compilation and Installation is Successful" 10 30
else
	echo "LAN865x driver compilation and installation is failed...!"
	dialog --title "Failed" --clear "$@" --msgbox "LAN865x Compilation and Installation is Failed" 10 30
fi
}

save_configuration(){
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
	cp lan865x-overlay-tmp.dts dts/lan865x-overlay.dts
	dos2unix dts/lan865x-overlay.dts &> /dev/null
	cp load-tmp.sh load.sh
	chmod +x load-tmp.sh
	dialog --title "Success" --clear "$@" --msgbox "LAN865x Configuration is Successful" 10 30
else
	dialog --title "Failed" --clear "$@" --msgbox "LAN865x Configuration is Failed" 10 30
fi
}

2nd_lan865x(){
returncode_2nd=0

while [ "${returncode_2nd:-99}" -ne 1 ] && [ "${returncode_2nd:-99}" -ne 250 ]; do
	exec 3>&1
v1="$(awk '{if(NR==28) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_mode_2=$v2

v1="$(awk '{if(NR==29) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_node_id_2=$v2

v1="$(awk '{if(NR==30) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_node_count_2=$v2

v1="$(awk '{if(NR==31) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_burst_count_2=$v2

v1="$(awk '{if(NR==32) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_burst_timer_2=$v2

v1="$(awk '{if(NR==33) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_to_timer_2=$v2

v1="$(awk '{if(NR==34) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
tx_cut_through_2=$v2

v1="$(awk '{if(NR==35) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
rx_cut_through_2=$v2

v1="$(awk '{if(NR==25) print $0}' lan865x-overlay-tmp.dts)"
v2=$(grep -oP '= \K.*?(?=;)' <<< "$v1")
v2=$(echo "$v2" | sed -r 's/[ ]+/:/g')
v2=${v2:1}
v2=${v2:0:17}
mac_addr_2=$v2

v1="$(awk '{if(NR==12) print $0}' load-tmp.sh)"
v2=$(cut -d "=" -f2- <<< $v1)
ip_addr_2=$v2

v1="$(awk '{if(NR==13) print $0}' load-tmp.sh)"
v2=$(cut -d "=" -f2 <<< "$v1")
subnet_mask_2=$v2

	value_2nd=`dialog \
	      --clear --ok-label "Save" --cancel-label "Back" --extra-label "Modify" \
	      --backtitle "This script is for configuring LAN865x in Raspberry Pi" "$@" \
	      --inputmenu "2nd LAN865x Configuration: [Note: Please do save in case any of the below parameter is modified otherwise the changes will be lost]" 45 75 10 \
	      "1. PLCA Mode (0-disable, 1-enable)"		"$plca_mode_2" \
	      "2. PLCA Node ID (0-254)"				"$plca_node_id_2" \
	      "3. PLCA Node Count (2-255)"			"$plca_node_count_2" \
	      "4. PLCA Burst Count (0x0-0xFF)"			"$plca_burst_count_2" \
	      "5. PLCA Burst Timer (0x0-0xFF)"			"$plca_burst_timer_2" \
	      "6. PLCA TO Timer (0x0-0xFF)"			"$plca_to_timer_2" \
	      "7. Tx Cut Through Mode (0-disable, 1-enable)"	"$tx_cut_through_2" \
	      "8. Rx Cut Through Mode (0-disable, 1-enable)"	"$rx_cut_through_2" \
	      "9. MAC Address (Ex format: 11:22:33:44:55:66)"	"$mac_addr_2" \
	      "10. IP Address (Ex format: 192.168.1.10)"	"$ip_addr_2" \
	      "11. Subnet Mask (Ex format: 255.255.255.0)"	"$subnet_mask_2" \
	      2>&1 1>&3 `

	      returncode_2nd=$?
	      exec 3>&-
	      case $returncode_2nd in
	      0) #Save
	      		save_configuration
	      ;;
	      1) #Cancel
		dialog \
			--clear --backtitle "This script is for configuring LAN865x in Raspberry Pi" \
			--yesno "Please ensure your changes are saved before going back to the main menu otherwise your changes will be lost. Do you want to proceed?" 10 40
		case $? in
		0) #OK
			break;;
		1) #Cancel
			returncode_2nd=99;;
		esac
	      ;;
	      3) #Change
		      value_2nd=`echo "$value_2nd" | sed -e 's/^RENAMED //'`
		      tag=`echo "$value_2nd" | sed -e 's/).*//'`
		      item=`echo "$value_2nd" | sed -e 's/^[^)]*)[ 	][ 	]*//'`

		      case "$tag" in
	      		"1. PLCA Mode (0-disable, 1-enable")
				if [[ $item == 0 ]] || [[ $item == 1 ]]
				then
				plca_mode_2=$item
				data="                                plca-enable = /bits/ 8 <$plca_mode_2>; /* 1 - PLCA enable, 0 - CSMA/CD enable */"
				awk -F"=" -v newval="$data" '{ if (NR == 28) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				else
				dialog --title "Error" --clear "$@" --msgbox "Invalid PLCA Mode Configuration" 10 30
				fi;;
	      		"2. PLCA Node ID (0-254")
				if [[ -n ${item//[0-9]/} ]];
				then
				dialog --title "Error" --clear "$@" --msgbox "Invalid PLCA Node ID Configuration" 10 30
				else
				if [[ $item -lt 0 ]] || [[ $item -gt 254 ]]
				then
				dialog --title "Error" --clear "$@" --msgbox "Invalid PLCA Node ID Configuration" 10 30
				else
				plca_node_id_2=$item
				data="                                plca-node-id = /bits/ 8 <$plca_node_id_2>; /* PLCA node id range: 0 to 254 */"
				awk -F"=" -v newval="$data" '{ if (NR == 29) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				fi
				fi;;
	      		"3. PLCA Node Count (2-255")
				if [[ -n ${item//[0-9]/} ]];
				then
				dialog --title "Error" --clear "$@" --msgbox "Invalid PLCA Node Count Configuration" 10 30
				else
				if [[ $item -lt 2 ]] || [[ $item -gt 255 ]]
				then
				dialog --title "Error" --clear "$@" --msgbox "Invalid PLCA Node Count Configuration" 10 30
				else
				plca_node_count_2=$item
				data="                                plca-node-count = /bits/ 8 <$plca_node_count_2>; /* PLCA node count range: 1 to 255 */"
				awk -F"=" -v newval="$data" '{ if (NR == 30) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				fi
				fi;;
	      		"4. PLCA Burst Count (0x0-0xFF")
				v1=${item:0:2}
				if [[ $v1 == "0x" ]]
				then
				item=${item:2}
				fi
				if [[ ${#item} -lt 1 ]] || [[ ${#item} -gt 2 ]] || ! [[ $item =~ ^[0-9A-Fa-f]{1,}$ ]]
				then
				dialog --title "Error" --clear "$@" --msgbox "Invalid PLCA Burst Count Configuration" 10 30
				else
				plca_burst_count_2=0x$item
				data="                                plca-burst-count = /bits/ 8 <$plca_burst_count_2>; /* PLCA burst count range: 0x0 to 0xFF */"
				awk -F"=" -v newval="$data" '{ if (NR == 31) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				fi;;
	      		"5. PLCA Burst Timer (0x0-0xFF")
				v1=${item:0:2}
				if [[ $v1 == "0x" ]]
				then
				item=${item:2}
				fi
				if [[ ${#item} -lt 1 ]] || [[ ${#item} -gt 2 ]] || ! [[ $item =~ ^[0-9A-Fa-f]{1,}$ ]]
				then
				dialog --title "Error" --clear "$@" --msgbox "Invalid PLCA Burst Timer Configuration" 10 30
				else
				plca_burst_timer_2=0x$item
				data="                                plca-burst-timer = /bits/ 8 <$plca_burst_timer_2>; /* PLCA burst timer */"
				awk -F"=" -v newval="$data" '{ if (NR == 32) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				fi;;
	      		"6. PLCA TO Timer (0x0-0xFF")
				v1=${item:0:2}
				if [[ $v1 == "0x" ]]
				then
				item=${item:2}
				fi
				if [[ ${#item} -lt 1 ]] || [[ ${#item} -gt 2 ]] || ! [[ $item =~ ^[0-9A-Fa-f]{1,}$ ]]
				then
				dialog --title "Error" --clear "$@" --msgbox "Invalid PLCA TO Timer Configuration" 10 30
				else
				plca_to_timer_2=0x$item
				data="                                plca-to-timer = /bits/ 8 <$plca_to_timer_2>; /* PLCA TO timer */"
				awk -F"=" -v newval="$data" '{ if (NR == 33) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				fi;;
	      		"7. Tx Cut Through Mode (0-disable, 1-enable")
				if [[ $item == 0 ]] || [[ $item == 1 ]]
				then
				tx_cut_through_2=$item
				data="                                tx-cut-through-mode = /bits/ 8 <$tx_cut_through_2>; /* 1 - Tx cut through mode enable, 0 - Store and forward mode enable */"
				awk -F"=" -v newval="$data" '{ if (NR == 34) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				else
				dialog --title "Error" --clear "$@" --msgbox "Invalid Tx Cut Through Mode Configuration" 10 30
				fi;;
	      		"8. Rx Cut Through Mode (0-disable, 1-enable")
				if [[ $item == 0 ]] || [[ $item == 1 ]]
				then
				rx_cut_through_2=$item
				data="                                rx-cut-through-mode = /bits/ 8 <$rx_cut_through_2>; /* 1 - Rx cut through mode enable, 0 - Store and forward mode enable */"
				awk -F"=" -v newval="$data" '{ if (NR == 35) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				else
				dialog --title "Error" --clear "$@" --msgbox "Invalid Rx Cut Through Mode Configuration" 10 30
				fi;;
	      		"9. MAC Address (Ex format: 11:22:33:44:55:66")
				if [[ $item =~ ^([[:xdigit:]]{2}:){5}[[:xdigit:]]{2}$ ]]; then
				mac_addr_2=$item
				mac_addr_2_tmp=$mac_addr_2
				mac_addr_2=$(echo "$mac_addr_2" | sed -r 's/[:]+/ /g')
				data="                                local-mac-address = [$mac_addr_2];"
				awk -F"=" -v newval="$data" '{ if (NR == 25) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				data="mac_addr_2=$mac_addr_2_tmp"
				awk -F"=" -v newval="$data" '{ if (NR == 11) print newval ; else print $0}' load-tmp.sh > load-tmp_tmp.sh
				mv load-tmp_tmp.sh load-tmp.sh
				chmod +x load-tmp.sh
				else
				dialog --title "Error" --clear "$@" --msgbox "Invalid MAC Address Configuration" 10 30
				fi;;
			"10. IP Address (Ex format: 192.168.1.10")
				if [[ "$item" =~ ^(([1-9]?[0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))\.){3}([1-9]?[0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))$ ]]; then
				ip_addr_2=$item
				data="ip_addr_2=$ip_addr_2"
				awk -F"=" -v newval="$data" '{ if (NR == 12) print newval ; else print $0}' load-tmp.sh > load-tmp_tmp.sh
				mv load-tmp_tmp.sh load-tmp.sh
				chmod +x load-tmp.sh
				else
				dialog --title "Error" --clear "$@" --msgbox "Invalid IP Address Configuration" 10 30
				fi;;
			"11. Subnet Mask (Ex format: 255.255.255.0")
				if [[ "$item" =~ ^(([1-9]?[0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))\.){3}([1-9]?[0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))$ ]]; then
				subnet_mask_2=$item
				data="subnet_mask_2=$subnet_mask_2"
				awk -F"=" -v newval="$data" '{ if (NR == 13) print newval ; else print $0}' load-tmp.sh > load-tmp_tmp.sh
				mv load-tmp_tmp.sh load-tmp.sh
				chmod +x load-tmp.sh
				else
				dialog --title "Error" --clear "$@" --msgbox "Invalid Subnet Mask Configuration" 10 30
				fi;;
		      esac
		;;
		255)
			dialog \
			--clear --backtitle "This script is for configuring LAN865x in Raspberry Pi" \
			--yesno "Please ensure your changes are saved before going back to the main menu otherwise your changes will be lost. Do you want to prceed?" 10 40
		case $? in
		0) #OK
			rm lan865x-overlay-tmp.dts
			rm load-tmp.sh
			clear
			break;;
		1) #Cancel
			returncode_2nd=99;;
		esac
		;;
		esac
	done
	clear
}


1st_lan865x(){
returncode_1st=0

while [ "${returncode_1st:-99}" -ne 1 ] && [ "${returncode_1st:-99}" -ne 250 ]; do
	exec 3>&1
v1="$(awk '{if(NR==49) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_mode_1=$v2

v1="$(awk '{if(NR==50) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_node_id_1=$v2

v1="$(awk '{if(NR==51) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_node_count_1=$v2

v1="$(awk '{if(NR==52) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_burst_count_1=$v2

v1="$(awk '{if(NR==53) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_burst_timer_1=$v2

v1="$(awk '{if(NR==54) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
plca_to_timer_1=$v2

v1="$(awk '{if(NR==55) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
tx_cut_through_1=$v2

v1="$(awk '{if(NR==56) print $5}' lan865x-overlay-tmp.dts)"
v2=$(echo $v1 | awk -F[='<''>'] '{print $2}')
rx_cut_through_1=$v2

v1="$(awk '{if(NR==46) print $0}' lan865x-overlay-tmp.dts)"
v2=$(grep -oP '= \K.*?(?=;)' <<< "$v1")
v2=$(echo "$v2" | sed -r 's/[ ]+/:/g')
v2=${v2:1}
v2=${v2:0:17}
mac_addr_1=$v2

v1="$(awk '{if(NR==8) print $0}' load-tmp.sh)"
v2=$(cut -d "=" -f2- <<< $v1)
ip_addr_1=$v2

v1="$(awk '{if(NR==9) print $0}' load-tmp.sh)"
v2=$(cut -d "=" -f2 <<< "$v1")
subnet_mask_1=$v2

	value_1st=`dialog \
	      --clear --ok-label "Save" --cancel-label "Back" --extra-label "Modify" \
	      --backtitle "This script is for configuring LAN865x in Raspberry Pi" "$@" \
	      --inputmenu "1st LAN865x Configuration: [Note: Please do save in case any of the below parameter is modified otherwise the changes will be lost]" 45 75 10 \
	      "1. PLCA Mode (0-disable, 1-enable)"		"$plca_mode_1" \
	      "2. PLCA Node ID (0-254)"				"$plca_node_id_1" \
	      "3. PLCA Node Count (2-255)"			"$plca_node_count_1" \
	      "4. PLCA Burst Count (0x0-0xFF)"			"$plca_burst_count_1" \
	      "5. PLCA Burst Timer (0x0-0xFF)"			"$plca_burst_timer_1" \
	      "6. PLCA TO Timer (0x0-0xFF)"			"$plca_to_timer_1" \
	      "7. Tx Cut Through Mode (0-disable, 1-enable)"	"$tx_cut_through_1" \
	      "8. Rx Cut Through Mode (0-disable, 1-enable)"	"$rx_cut_through_1" \
	      "9. MAC Address (Ex format: 11:22:33:44:55:66)"	"$mac_addr_1" \
	      "10. IP Address (Ex format: 192.168.1.10)"	"$ip_addr_1" \
	      "11. Subnet Mask (Ex format: 255.255.255.0)"	"$subnet_mask_1" \
	      2>&1 1>&3 `

	      returncode_1st=$?
	      exec 3>&-
	      case $returncode_1st in
	      0) #Save
	      		save_configuration
	      ;;
	      1) #Cancel
		dialog \
			--clear --backtitle "This script is for configuring LAN865x in Raspberry Pi" \
			--yesno "Please ensure your changes are saved before going back to the main menu otherwise your changes will be lost. Do you want to proceed?" 10 40
		case $? in
		0) #OK
			break;;
		1) #Cancel
			returncode_1st=99;;
		esac
		;;
	      3) #Change
		      value_1st=`echo "$value_1st" | sed -e 's/^RENAMED //'`
		      tag=`echo "$value_1st" | sed -e 's/).*//'`
		      item=`echo "$value_1st" | sed -e 's/^[^)]*)[ 	][ 	]*//'`
			#dialog --title "MESSAGE BOX" --clear "$@" --msgbox "$value_1st" 0 0

		      case "$tag" in
	      		"1. PLCA Mode (0-disable, 1-enable")	        
				if [[ $item == 0 ]] || [[ $item == 1 ]]
				then
				plca_mode_1=$item
				data="                                plca-enable = /bits/ 8 <$plca_mode_1>; /* 1 - PLCA enable, 0 - CSMA/CD enable */"
				awk -F"=" -v newval="$data" '{ if (NR == 49) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				else
				dialog --title "Error" --clear "$@" --msgbox "Invalid PLCA Mode Configuration" 10 30
				fi;;
	      		"2. PLCA Node ID (0-254")		        
				if [[ -n ${item//[0-9]/} ]];
				then
				dialog --title "Error" --clear "$@" --msgbox "Invalid PLCA Node ID Configuration" 10 30
				else
				if [[ $item -lt 0 ]] || [[ $item -gt 254 ]]
				then
				dialog --title "Error" --clear "$@" --msgbox "Invalid PLCA Node ID Configuration" 10 30
				else
				plca_node_id_1=$item
				data="                                plca-node-id = /bits/ 8 <$plca_node_id_1>; /* PLCA node id range: 0 to 254 */"
				awk -F"=" -v newval="$data" '{ if (NR == 50) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				fi
				fi;;
	      		"3. PLCA Node Count (2-255")			
				if [[ -n ${item//[0-9]/} ]];
				then
				dialog --title "Error" --clear "$@" --msgbox "Invalid PLCA Node Count Configuration" 10 30
				else
				if [[ $item -lt 2 ]] || [[ $item -gt 255 ]]
				then
				dialog --title "Error" --clear "$@" --msgbox "Invalid PLCA Node Count Configuration" 10 30
				else
				plca_node_count_1=$item
				data="                                plca-node-count = /bits/ 8 <$plca_node_count_1>; /* PLCA node count range: 1 to 255 */"
				awk -F"=" -v newval="$data" '{ if (NR == 51) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				fi
				fi;;
	      		"4. PLCA Burst Count (0x0-0xFF")
				v1=${item:0:2}
				if [[ $v1 == "0x" ]]
				then
				item=${item:2}
				fi
				if [[ ${#item} -lt 1 ]] || [[ ${#item} -gt 2 ]] || ! [[ $item =~ ^[0-9A-Fa-f]{1,}$ ]]
				then
				dialog --title "Error" --clear "$@" --msgbox "Invalid PLCA Burst Count Configuration" 10 30
				else
				plca_burst_count_1=0x$item
				data="                                plca-burst-count = /bits/ 8 <$plca_burst_count_1>; /* PLCA burst count range: 0x0 to 0xFF */"
				awk -F"=" -v newval="$data" '{ if (NR == 52) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				fi;;
	      		"5. PLCA Burst Timer (0x0-0xFF")		
				v1=${item:0:2}
				if [[ $v1 == "0x" ]]
				then
				item=${item:2}
				fi
				if [[ ${#item} -lt 1 ]] || [[ ${#item} -gt 2 ]] || ! [[ $item =~ ^[0-9A-Fa-f]{1,}$ ]]
				then
				dialog --title "Error" --clear "$@" --msgbox "Invalid PLCA Burst Timer Configuration" 10 30
				else
				plca_burst_timer_1=0x$item
				data="                                plca-burst-timer = /bits/ 8 <$plca_burst_timer_1>; /* PLCA burst timer */"
				awk -F"=" -v newval="$data" '{ if (NR == 53) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				fi;;
	      		"6. PLCA TO Timer (0x0-0xFF")			
				v1=${item:0:2}
				if [[ $v1 == "0x" ]]
				then
				item=${item:2}
				fi
				if [[ ${#item} -lt 1 ]] || [[ ${#item} -gt 2 ]] || ! [[ $item =~ ^[0-9A-Fa-f]{1,}$ ]]
				then
				dialog --title "Error" --clear "$@" --msgbox "Invalid PLCA TO Timer Configuration" 10 30
				else
				plca_to_timer_1=0x$item
				data="                                plca-to-timer = /bits/ 8 <0x$plca_to_timer_1>; /* PLCA TO timer */"
				awk -F"=" -v newval="$data" '{ if (NR == 54) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				fi;;

	      		"7. Tx Cut Through Mode (0-disable, 1-enable")
				if [[ $item == 0 ]] || [[ $item == 1 ]]
				then
				tx_cut_through_1=$item
				data="                                tx-cut-through-mode = /bits/ 8 <$tx_cut_through_1>; /* 1 - Tx cut through mode enable, 0 - Store and forward mode enable */"
				awk -F"=" -v newval="$data" '{ if (NR == 55) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				else
				dialog --title "Error" --clear "$@" --msgbox "Invalid Tx Cut Through Mode Configuration" 10 30
				fi;;
	      		"8. Rx Cut Through Mode (0-disable, 1-enable")
				if [[ $item == 0 ]] || [[ $item == 1 ]]
				then
				rx_cut_through_1=$item
				data="                                rx-cut-through-mode = /bits/ 8 <$rx_cut_through_1>; /* 1 - Rx cut through mode enable, 0 - Store and forward mode enable */"
				awk -F"=" -v newval="$data" '{ if (NR == 56) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				else
				dialog --title "Error" --clear "$@" --msgbox "Invalid Rx Cut Through Mode Configuration" 10 30
				fi;;
	      		"9. MAC Address (Ex format: 11:22:33:44:55:66")
				if [[ $item =~ ^([[:xdigit:]]{2}:){5}[[:xdigit:]]{2}$ ]]; then
				mac_addr_1=$item
				mac_addr_1_tmp=$mac_addr_1
				mac_addr_1=$(echo "$mac_addr_1" | sed -r 's/[:]+/ /g')
				data="                                local-mac-address = [$mac_addr_1];"
				awk -F"=" -v newval="$data" '{ if (NR == 46) print newval ; else print $0}' lan865x-overlay-tmp.dts > lan865x-overlay-tmp-tmp.dts
				mv lan865x-overlay-tmp-tmp.dts lan865x-overlay-tmp.dts
				data="mac_addr_1=$mac_addr_1_tmp"
				awk -F"=" -v newval="$data" '{ if (NR == 7) print newval ; else print $0}' load-tmp.sh > load-tmp_tmp.sh
				mv load-tmp_tmp.sh load-tmp.sh
				chmod +x load-tmp.sh
				else
				dialog --title "Error" --clear "$@" --msgbox "Invalid MAC Address Configuration" 10 30
				fi;;
			"10. IP Address (Ex format: 192.168.1.10")
				if [[ "$item" =~ ^(([1-9]?[0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))\.){3}([1-9]?[0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))$ ]]; then
				ip_addr_1=$item
				data="ip_addr_1=$ip_addr_1"
				awk -F"=" -v newval="$data" '{ if (NR == 8) print newval ; else print $0}' load-tmp.sh > load-tmp_tmp.sh
				mv load-tmp_tmp.sh load-tmp.sh
				chmod +x load-tmp.sh
				else
				dialog --title "Error" --clear "$@" --msgbox "Invalid IP Address Configuration" 10 30
				fi;;
			"11. Subnet Mask (Ex format: 255.255.255.0")
				if [[ "$item" =~ ^(([1-9]?[0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))\.){3}([1-9]?[0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))$ ]]; then
				subnet_mask_1=$item
				data="subnet_mask_1=$subnet_mask_1"
				awk -F"=" -v newval="$data" '{ if (NR == 9) print newval ; else print $0}' load-tmp.sh > load-tmp_tmp.sh
				mv load-tmp_tmp.sh load-tmp.sh
				chmod +x load-tmp.sh
				else
				dialog --title "Error" --clear "$@" --msgbox "Invalid Subnet Mask Configuration" 10 30
				fi;;
		      esac
		;;
		255)
			dialog \
			--clear --backtitle "This script is for configuring LAN865x in Raspberry Pi" \
			--yesno "Please ensure your changes are saved before going back to the main menu otherwise your changes will be lost. Do you want to prceed?" 10 40
		case $? in
		0) #OK
			rm lan865x-overlay-tmp.dts
			rm load-tmp.sh
			clear
			break;;
		1) #Cancel
			returncode_1st=99;;
		esac
		;;
	esac
	done
	clear
}

dpkg -s "dialog" &> /dev/null
if [ $? != 0 ]
then
echo "Package dialog is not installed...!"
echo "Do you want to install it ?"
echo " 	1. Yes
	0. No"
echo "Enter your option:"
read option
if [ $option == 1 ]
then
	sudo apt-get install dialog
elif [ $option == 0 ]
then
	echo "Please install dialog and try again...!"
	exit 0
else
	echo "Invalid input...!"
	exit 0
fi
fi

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

while [ "${returncode:-99}" -ne 1 ] && [ "${returncode:-99}" -ne 250 ]; do
	exec 3>&1
	value=`dialog --clear --help-button --ok-label "Select" --cancel-label "Exit" --backtitle "This script is for configuring LAN865x in Raspberry Pi" --title "[ LAN865x Configuration Options ]" --menu "You can use the UP/DOWN arrow keys, the first letter of the choice as a hot key, or the number keys 1-3 to choose an option. Choose the option" 0 0 3 "1. Configure 1st LAN865x" "Configuration options for 1st LAN865x" "2. Configure 2nd LAN865x" "Configuration options for 2nd LAN865x" "3. Install Driver" "Option for compiling and installing the driver" 2>&1 1>&3 `

	returncode=$?
	exec 3>&-
	cp dts/lan865x-overlay.dts lan865x-overlay-tmp.dts
	cp load.sh load-tmp.sh
	case $returncode in
	0) #Ok
		case $value in
		"1. Configure 1st LAN865x")	1st_lan865x;;
		"2. Configure 2nd LAN865x")	2nd_lan865x;;
		"3. Install Driver")		install_driver;;
		esac
		;;
	1) #Cancel
		dialog \
			--clear --backtitle "This script is for configuring LAN865x in Raspberry Pi" \
			--yesno "Do you want to exit?" 10 40
		case $? in
		0) #OK
			rm lan865x-overlay-tmp.dts
			rm load-tmp.sh
			clear
			break;;
		1) #Cancel
			returncode=99;;
		esac
	;;
	2) #Help
	dialog --title "Help" --clear "$@" --msgbox "This script is for configuring LAN865x in Raspberry Pi" 10 30
	;;
	255) #Escape
		dialog \
			--clear --backtitle "This script is for configuring LAN865x in Raspberry Pi" \
			--yesno "Do you want to exit?" 10 40
		case $? in
		0) #OK
			rm lan865x-overlay-tmp.dts
			rm load-tmp.sh
			clear
			break;;
		1) #Cancel
			returncode=99;;
		esac
	;;
	esac
	rm lan865x-overlay-tmp.dts
	rm load-tmp.sh
	clear
done
