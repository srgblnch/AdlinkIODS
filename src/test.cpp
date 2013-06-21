

/****************************************************************************/
/* Calculo del mcd de dos enteros sin divisiones, solo restas y             */
/* desplazamientos.                                                         */
/*                                                                          */
/* Jaime Suarez <mcripto@bigfoot.com> 2003                                  */
/* en http://www.matematicas.net                                            */
/****************************************************************************/

#define PAR(X) !(X&1)

#include <cstdio>
#include <cassert>
#include <iostream>
#include <d2kdask.h>
#include "AdlDeviceFactorySingleton.cpp"

long mcd(long,long);

// main(int argc, char *argv[])
// {
// 	long a,b;
// 
// 	if (argc!=3) {
// 		printf("%s <a> <b> calcula el mcd de a y b.\n",argv[0]);
// 		return 1;
// 	}
// 	a=atol(argv[1]); b=atol(argv[2]);
// 	printf("El m.c.d. de %ld y %ld es %ld\n",a,b,mcd(a,b));
// 	return 0;
// }

long mcd(long a, long b)
{
	if (a==b) return a;
	else if (a<b) return mcd(b,a);
	else if ( PAR(a) &&  PAR(b))  return 2*mcd(a>>1,b>>1);
	else if ( PAR(a) && !PAR(b))  return mcd(a>>1,b);
	else if (!PAR(a) &&  PAR(b))  return mcd(a,b>>1);
	else                          return mcd(a-b,b);
}

	

long adaptBufferSize(long chBufSz, long nCh, long fifoSz)
{
	long bufSz = chBufSz * nCh;
	double origProportion = double(bufSz) / fifoSz;

	assert(fifoSz % 4 == 0);

	// bufSz % fifoSz == 0
	// bufSz % nCh == 0
	// bufSz % mcm(fifoSz, nCh) == 0
	// mcm(a,b) = (a*b) / mcd(a,b)

	long aux = fifoSz * nCh / mcd(fifoSz, nCh);
	long l3 = bufSz  + ( ( aux - (bufSz % aux) ) % aux);

	double proportion = double(l3) / fifoSz;
	
	std::cout << "nCh: " << nCh << std::endl;
	std::cout << "fifoSz: " << fifoSz << std::endl;
	std::cout << "ORIGINAL: " << origProportion << std::endl;
	std::cout << "CHBFSZ: " << chBufSz << std::endl;
	std::cout << "ARA: " << proportion << std::endl;
	std::cout << "CHBFSZ: " << (double(l3) / nCh) << std::endl;
	std::cout << "-~-~-~-~-~-~-~-~-~-~-~" << std::endl;
	
	return bufSz / fifoSz;
}

/*
g++ -g -D_REENTRANT -I/homelocal/sicilia/tango/include   -I/home/sicilia/AbstractClasses/AnalogDAQ/ -I/homelocal/sicilia/omniorb/include -I/usr/local/adlink/include -I./include -I. -I/homelocal/sicilia/tango/cppserver/include -DADTANGO_USE_POLLING_THREADS    test.cpp   -o test -lpci_dask2k -L/home/developer/deltadask/lib/ -lpci_dask -lpthread
*/

int main()
{
// 	adaptBufferSize(500, 8, 256);
// 	adaptBufferSize(1783, 9, 256);
// 	adaptBufferSize(1783, 63, 512);
// 	adaptBufferSize(1783, 63, 8192);
// 	adaptBufferSize(100000, 63, 8192);


// 	std::cout << "Register " << D2K_Register_Card(DAQ_2010, 0) << std::endl;
// 	std::cout << "Register " << D2K_Register_Card(DAQ_2010, 0) << std::endl;
// 	std::cout << "Register " << D2K_Register_Card(DAQ_2010, 0) << std::endl;
	
	namespace adm = AdlDeviceFactorySingleton;
	using adm::AdlBoardParams;
	
	const AdlBoardParams *params;
	
	params = adm::get_board_params("DAQ_2010");
	I16 num, num2;
	bool r;
	
	num = adm::get(params, 0, adm::ModeAnalog);
	std::cout << "Regitrat " << num << std::endl;
	
	num2 = adm::get(params, 0, adm::ModeDigital);
	std::cout << "Regitrat " << num2 << std::endl;
	
	num2 = adm::get(params, 0, adm::ModeDigital);
	std::cout << "Regitrat " << num2 << std::endl;
	
	r = adm::release(params, 0, adm::ModeDigital);
	std::cout << "Desregistra " << r << std::endl;
	
	r = adm::release(params, 0, adm::ModeDigital);
	std::cout << "Desregistra " << r << std::endl;
	
	r = adm::release(params, 0, adm::ModeAnalog);
	std::cout << "Desregistra " << r << std::endl;
}

