#!/bin/bash

copy_mac_dsym() {
	pdb_path=${PROJECT_DIR}/pdb/Symbol/${VERSION}/${BUILD_TYPE}/${ARCH}

	status "copy mac dsym to ${pdb_path}"
	trap "caught_error 'copy mac dsym'" ERR
	
	ensure_dir ${pdb_path}

	cd ${PROJECT_DIR}/src
	status `find ./ -name "*.dSYM"  | tr " " "\?"`
	for file in `find ./ -name "*.dSYM"  | tr " " "\?"`;
	do
		cp -R "$file" "${pdb_path}"; 
		status "copy dsym path: ${file}";
	done
}


copy_mac_dsym_standalone() {

	status "_RUN_PRISM_BUILD_SCRIPT=${_RUN_PRISM_BUILD_SCRIPT}"

    if [ -z "${_RUN_PRISM_BUILD_SCRIPT}" ]; then
    	SCRIPT_PATH=$( cd "$(dirname "${BASH_SOURCE[0]}")" && pwd);
    	PROJECT_DIR=${SCRIPT_PATH}/../../
    	BUILD_TYPE=Debug
    	ARCH=arm64
		verson_file_path=${SCRIPT_PATH}/version_mac.txt
		VERSION=$(sed '1!d' ${verson_file_path})
    	source "${SCRIPT_PATH}/prism_build_support_macos.sh"
        copy_mac_dsym
    fi
}

copy_mac_dsym_standalone $*