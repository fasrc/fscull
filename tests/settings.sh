#script also set these, but add here to help be sure
set -e
set -u


#--- time intervals in seconds

RETENTION_WINDOW=$(( 60 * 60 * 24 ))  #24 hours

AGE_KEEPME=$(( RETENTION_WINDOW / 2 ))  #1/2 the retention window
AGE_DELETEME=$(( RETENTION_WINDOW / 2 * 3 ))  #3/2 the retention window


#--- timestamps in seconds since the epoch

T_NOW=$(date +%s)
T_KEEPME=$(( T_NOW - AGE_KEEPME ))
T_DELETEME=$(( T_NOW - AGE_DELETEME ))


#--- dates in a human readable form that touch also understands

D_NOW=$(     date -d @$T_NOW      +'%Y-%m-%d %H:%M:%S')
D_KEEPME=$(  date -d @$T_KEEPME   +'%Y-%m-%d %H:%M:%S')
D_DELETEME=$(date -d @$T_DELETEME +'%Y-%m-%d %H:%M:%S')
