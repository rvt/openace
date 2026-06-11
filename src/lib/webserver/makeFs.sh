# Script that can be run from this directory to generate the needed gatas_fsdata.c file for teh WebGUI
dir=$(pwd)
python3 $dir/external/makefs.py -d $dir/../../SystemGUI/dist -o $dir/gatas_fsdata.c

