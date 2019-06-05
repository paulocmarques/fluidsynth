# Try to find the READLINE library
#  HAVE_READLINE - system has READLINE
#  READLINE_INCLUDE_DIR - READLINE include directory
#  READLINE_LIBRARIES - Libraries needed to use READLINE

if ( READLINE_INCLUDE_DIR AND READLINE_LIBRARIES )
    set ( READLINE_FIND_QUIETLY TRUE )
endif ( READLINE_INCLUDE_DIR AND READLINE_LIBRARIES )

find_path ( READLINE_INCLUDE_DIR NAMES history.h readline/history.h )
find_library ( READLINE_LIBRARIES NAMES readline )

if ( READLINE_INCLUDE_DIR AND READLINE_LIBRARIES )
    set ( HAVE_READLINE TRUE CACHE BOOL )
endif ( READLINE_INCLUDE_DIR AND READLINE_LIBRARIES )

include ( FindPackageHandleStandardArgs )
FIND_PACKAGE_HANDLE_STANDARD_ARGS( READLINE DEFAULT_MSG 
                                   READLINE_INCLUDE_DIR 
                                   READLINE_LIBRARIES )

mark_as_advanced( READLINE_INCLUDE_DIR READLINE_LIBRARIES HAVE_READLINE )
