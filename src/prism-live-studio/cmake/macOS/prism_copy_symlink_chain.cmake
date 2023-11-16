#[===[
Params:
    LIBRARY     : the name of the symlink to the lib
    DESTINATION : the directory to copy the files to
]===]

file(COPY "${LIBRARY}" DESTINATION "${DESTINATION}" FOLLOW_SYMLINK_CHAIN)