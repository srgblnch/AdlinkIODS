#=============================================================================
#
# file :        Makefile
#
# description : Include for the AdlinkIODS class.
#
# project :     Makefile to generate a Tango server
#
# $Author: rsune $
#
# $Revision: 8648 $
#
# $Log: $
#
# copyleft :    CELLS/ALBA
#		Edifici Ciences Nord
#		Campus Universitari de Bellaterra
#		Universitat Autonoma de Barcelona
#		08193 Bellaterra, Barcelona, SPAIN
#
#=============================================================================
#  		This file is generated by POGO
#	(Program Obviously used to Generate tango Object)
#
#=============================================================================
#

CLASS      = AdlinkIODS
MAJOR_VERS = 1
MINOR_VERS = 0
RELEASE    = Release_$(MAJOR_VERS)_$(MINOR_VERS)

#-----------------------------------------
#	Set default home directories
#-----------------------------------------

TANGO_HOME    ?=  /homelocal/sicilia/tango
OMNI_HOME     ?=  /homelocal/sicilia/omniorb
DASK_HOME     ?=  /homelocal/sicilia/drivers/adlink
SUPER_HOME    ?=  /homelocal/sicilia/AbstractClasses/AnalogDAQ/
#DIOSUPER_HOME ?=  /siciliarep/backups/rsunepc/projects/tango_ds/InputOutput/DigitalIO/abstract/
DIOSUPER_HOME ?=  /siciliarep/build/svnco/tango_ds/InputOutput/DigitalIO/abstract/
GSL_HOME      ?= /usr

CPP_SERVERS  =  $(TANGO_HOME)/cppserver

D2K_LIB ?= pci_dask2k

64bits=$(shell getos | grep _64$ | wc -w)
DESTOS=$(shell getos)


ifdef no_debug
	DEBUG = -O2 -DNDEBUG
else
	DEBUG = -g -DDEBUG_ADLINK_DEVICE
endif

ifeq ("$(64bits)", "1")
 GSL_LIB_HOME = $(GSL_HOME)/lib64
 else
 GSL_LIB_HOME = $(GSL_HOME)/lib
endif


ifdef _solaris
CC = CC 
BIN_DIR=$(OS)_CC

AR_SL = $(CC) -mt -G
VERS_OPT = -h
SL_EXT = so
endif

ifdef linux
CC = c++
CC_SHLIB = $(CC)  -fPIC
AR = ar
BIN_DIR=$(OS)

AR_SL = $(CC) -fPIC -shared 
SL_EXT = so
VERS_OPT = -Wl,-soname,
endif

INCLUDE_DIRS =  -I$(TANGO_HOME)/include/tango \
                -I$(OMNI_HOME)/include \
		-I$(DASK_HOME)/include \
		-I. \
		-I./include \
		-I./aio \
		-I./dio \
		-I./counter \
		-I$(GSL_HOME) \
		-I$(SUPER_HOME)/include \
		-I$(DIOSUPER_HOME)/include \
		-I$(CPP_SERVERS)/include

OBJS_DIR    = obj-$(DESTOS)
LIB_DIRS    = -L$(TANGO_HOME)/lib \
		-L$(OMNI_HOME)/lib \
		-L$(DASK_HOME)/lib 

TARGET_LIB = $(CPP_SERVERS)/lib/libtgclasses.a


#-----------------------------------------
#	Set  CXXFLAGS and LFLAGS
#-----------------------------------------
ifdef _solaris
CXXFLAGS =  $(DEBUG) -mt -D_PTHREADS $(INCLUDE_DIRS)
LFLAGS =  $(DEBUG) $(LIB_DIRS)  		\
				-ltango			\
				-llog4tango		\
				-lomniORB4 		\
				-lomniDynamic4	\
				-lomnithread	\
				-lCOS4			\
				-lpthread		\
				-lposix4 -lsocket -lnsl
endif

ifdef linux
CXXFLAGS =  $(DEBUG) -D_REENTRANT $(INCLUDE_DIRS)
LFLAGS =  $(DEBUG) $(LIB_DIRS)  		\
				-ltango			\
				-llog4tango		\
				-lomniORB4 		\
				-lomniDynamic4	\
				-lomnithread	\
				-lCOS4			\
				-ldl -lpthread \
				-l$(D2K_LIB) \
				-lpci_dask \
				-lrt
endif




#-----------------------------------------
#	Set  dependences
#-----------------------------------------

SUPER_OBJS = $(SUPER_HOME)/obj/AnalogDAQClass.o \
		$(SUPER_HOME)/obj/AnalogDAQStateMachine.o

SVC_OBJS = $(OBJS_DIR)/main.o	\
		$(OBJS_DIR)/ClassFactory.o

SHLIB_OBJS = $(OBJS_DIR)/$(CLASS)Class.so.o	\
		$(OBJS_DIR)/$(CLASS)StateMachine.so.o	\
		$(OBJS_DIR)/$(CLASS).so.o


SVC_INC =


$(OBJS_DIR)/%.o: %.cpp $(SVC_INC)
	$(CC) $(CXXFLAGS) -c $< -o $(OBJS_DIR)/$*.o
			
$(OBJS_DIR)/%.so.o: %.cpp $(SVC_INC)
	$(CC_SHLIB) $(CXXFLAGS) -c $< -o $(OBJS_DIR)/$*.so.o


#-----------------------------------------
#	 Make Entry
#-----------------------------------------

#all: $(CLASS)
all: bin/AdlinkIODS.$(DESTOS)

$(CLASS):	make_obj_dir make_bin_dir $(SVC_OBJS)
	$(CC) $(SVC_OBJS) -o $(CLASS) $(LFLAGS)
	@mv $(CLASS) bin/$(CLASS)

shlib:	make_obj_dir make_shlib_dir $(SHLIB_OBJS)
	$(AR_SL) -o \
		shlib/$(CLASS).$(SL_EXT).$(MAJOR_VERS).$(MINOR_VERS) \
		$(VERS_OPT)$(CLASS).$(SL_EXT).$(MAJOR_VERS) \
		$(SHLIB_OBJS) $(LFLAGS)
	@rm -f shlib/$(CLASS).$(SL_EXT)
	@cd  shlib/$(BIN_DIR); \
		ln -s $(CLASS).$(SL_EXT).$(MAJOR_VERS).$(MINOR_VERS) $(CLASS).$(SL_EXT)


aio/AdlinkAIO-$(DESTOS).a: aio/*.h aio/*.cpp
	cd aio && make lib

dio/AdlinkDIO-$(DESTOS).a: dio/*.h dio/*.cpp
	cd dio && SUPER_HOME="$(DIOSUPER_HOME)" make lib

counter/AdlinkIOCounter-$(DESTOS).a: counter/*.h counter/*.cpp
	cd counter && make lib

src/utils-$(DESTOS).a: src/*.cpp
	cd src && make lib

aux-$(DESTOS).a:  aio/AdlinkAIO-$(DESTOS).a dio/AdlinkDIO-$(DESTOS).a src/utils-$(DESTOS).a counter/AdlinkIOCounter-$(DESTOS).a 
	
link:
	cd bin && rm -f AdlinkIODS && ln -s AdlinkIODS.$(DESTOS) AdlinkIODS

bin/AdlinkIODS.$(DESTOS):  make_obj_dir make_bin_dir aux-$(DESTOS).a $(SVC_OBJS)
	$(CC) $(SUPER_OBJS) $(SVC_OBJS) -o bin/AdlinkIODS.$(DESTOS) $(LFLAGS) ./aio/AdlinkAIO-$(DESTOS).a ./dio/AdlinkDIO-$(DESTOS).a   ./src/utils-$(DESTOS).a $(DIOSUPER_HOME)/lib/libtgclasses.a ./aio/AdlinkAIO-$(DESTOS).a ./counter/AdlinkIOCounter-$(DESTOS).a $(GSL_LIB_HOME)/libgsl.a $(GSL_LIB_HOME)/libgslcblas.a
	# todo I am statically linking lgsl and lgslblas because they are not
	# part of the standard installation here at cells, I should tell
	# systems to change this...
	# -lgsl and -lgslblas are a dependency of aio 

todo:
	find -iname \*.cpp -o -iname \*.h | xargs -n1 grep -H todo --color

doc-make:
	cd doc && make

doc-clean:
	cd doc && make clean

doc: doc-make

cleanall: doc-clean clean

clean-%:
	cd $* && make clean 

clean: clean-aio clean-src clean-dio clean-counter
	rm -f $(OBJS_DIR)/*.o  \
	$(OBJS_DIR)/*.so.o \
	bin/$(CLASS).$(DESTOS) \
	*~ \
	core

make_obj_dir:
	@mkdir -p obj-$(DESTOS)

make_bin_dir:
	@mkdir -p bin

make_shlib_dir:
	@mkdir -p shlib
	
#-----------------------------------------
#	 Install binary file
#-----------------------------------------
install: all
	@mkdir -p $(prefix)/bin
	cp bin/$(CLASS).$(DESTOS)  $(prefix)/bin/$(CLASS)

#-----------------------------------------
#	 Update class library and header files
#	recompile without debug mode.
#-----------------------------------------
#lib: clean
#	@make no_debug=1
#	cp *.h $(CPP_SERVERS)/include
#	ar ruv $(TARGET_LIB) $(CLASS).o
#	ar ruv $(TARGET_LIB) $(CLASS)Class.o
#	ar ruv $(TARGET_LIB) $(CLASS)StateMachine.o
#	ident  $(TARGET_LIB) | grep $(CLASS)

lib: make_lib_dir make_include_dir $(SVC_OBJS)
	cp *.h SharedBuffer.cpp $(DESTLIB_HOME)/include
	ar rc $(DESTLIB_HOME)/lib/AdlinkIODS.a $(SVC_OBJS)

make_lib_dir:
	@mkdir -p $(DESTLIB_HOME)/lib

make_include_dir:
	@mkdir -p $(DESTLIB_HOME)/include



## UNIT TESTING


CircularPositionTest: CircularPositionTest.cpp CircularPosition.cpp CircularPosition.h
	moc CircularPositionTest.cpp > CircularPositionTest.moc
	g++ -g CircularPosition.cpp CircularPositionTest.cpp `pkg-config QtTest --cflags --libs` `pkg-config QtCore --cflags --libs` -o CircularPositionTest -I/sicilia/omniorb/include -L/sicilia/omniorb/lib -lomnithread

test: CircularPositionTest
	echo Running test CircularPositionTest
	./CircularPositionTest


#----------------------------------------------------
#	Tag the CVS module corresponding to this class
#----------------------------------------------------
tag:
	@cvstag "$(CLASS)-$(RELEASE)"
	@make   $(CLASS)
	@make show_tag

show_tag:
	@cvstag -d 
