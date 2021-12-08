set -x

rm build.log
rm build_verbose.log

config_files=`find . -name platformio.ini`

dir_save=`pwd`
for config_file in ${config_files}; do
    dir=`dirname ${config_file}`
    echo "Building in ${dir}" >> build.log
    cd ${dir}
    rm -rf  ~/arduino_libs/esp32dev/
    rm -rf .pio
    platformio run
    if [ $? -ne 0 ]; then
        echo "Build FAILED in ${dir}" >> ../build.log
        exit
    fi
    cd ${dir_save}
done

find . -name platformio.ini | wc -l
find . -name "*.elf" | wc -l

