export TCODDIR=$(dirname $0)
export LD_LIBRARY_PATH=../lib/libtcod-1.15.1_build/lib:${LD_LIBRARY_PATH}
cd ${TCODDIR} && ./impact_bin $*
