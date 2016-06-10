FIND_PATH(ARTOOLKIT_INCLUDE_DIR AR/ar.h
  PATHS
  $ENV{ARTOOLKIT_HOME}
  NO_DEFAULT_PATH
    PATH_SUFFIXES include
)

FIND_PATH(ARTOOLKIT_INCLUDE_DIR AR/ar.h
  PATHS
  /usr/local/include
  /usr/include
  /sw/include # Fink
  /opt/local/include # DarwinPorts
  /opt/csw/include # Blastwave
  /opt/include
)

SET(ARTOOLKIT_LIBRARIES)

FIND_LIBRARY(AR_LIBRARY 
  NAMES AR
  PATHS $ENV{ARTOOLKIT_HOME}
    NO_DEFAULT_PATH
    PATH_SUFFIXES lib64 lib
)
FIND_LIBRARY(AR_LIBRARY 
  NAMES AR
  PATHS
    /usr/local
    /usr
    /sw
    /opt/local
    /opt/csw
    /opt
    /usr/freeware
  PATH_SUFFIXES lib64 lib
)

FIND_LIBRARY(ARICP_LIBRARY 
  NAMES ARICP
    PATHS $ENV{ARTOOLKIT_HOME}
        NO_DEFAULT_PATH
	    PATH_SUFFIXES lib64 lib
	    )
FIND_LIBRARY(ARICP_LIBRARY 
  NAMES ARICP
    PATHS
        /usr/local
	    /usr
	        /sw
		    /opt/local
		        /opt/csw
			    /opt
			        /usr/freeware
				  PATH_SUFFIXES lib64 lib
				  )

FIND_LIBRARY(ARVIDEO_LIBRARY 
  NAMES ARvideo
    PATHS $ENV{ARTOOLKIT_HOME}
        NO_DEFAULT_PATH
	    PATH_SUFFIXES lib64 lib
	    )
FIND_LIBRARY(ARVIDEO_LIBRARY 
  NAMES ARvideo
    PATHS
        /usr/local
	    /usr
	        /sw
		    /opt/local
		        /opt/csw
			    /opt
			        /usr/freeware
				  PATH_SUFFIXES lib64 lib
				  )

FIND_LIBRARY(ARGSUB_LIBRARY 
  NAMES ARgsub
    PATHS $ENV{ARTOOLKIT_HOME}
        NO_DEFAULT_PATH
	    PATH_SUFFIXES lib64 lib
	    )
FIND_LIBRARY(ARGSUB_LIBRARY 
  NAMES ARgsub
    PATHS
        /usr/local
	    /usr
	        /sw
		    /opt/local
		        /opt/csw
			    /opt
			        /usr/freeware
				  PATH_SUFFIXES lib64 lib
				  )

SET(ARTOOLKIT_LIBRARIES ${ARTOOLKIT_LIBRARIES} ${ARGSUB_LIBRARY})
SET(ARTOOLKIT_LIBRARIES ${ARTOOLKIT_LIBRARIES} ${ARVIDEO_LIBRARY})
SET(ARTOOLKIT_LIBRARIES ${ARTOOLKIT_LIBRARIES} ${AR_LIBRARY})
SET(ARTOOLKIT_LIBRARIES ${ARTOOLKIT_LIBRARIES} ${ARICP_LIBRARY})
SET(ARTOOLKIT_LIBRARIES ${ARTOOLKIT_LIBRARIES} ${AR_LIBRARY})

SET(ARTOOLKIT_FOUND "NO")
IF(ARTOOLKIT_LIBRARIES AND ARTOOLKIT_INCLUDE_DIR)
  SET(ARTOOLKIT_FOUND "YES")
ENDIF(ARTOOLKIT_LIBRARIES AND ARTOOLKIT_INCLUDE_DIR)

