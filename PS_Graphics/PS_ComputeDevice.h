#ifndef PS_COMPUTEDEVICE_H
#define PS_COMPUTEDEVICE_H
#include <vector>
#include <string>
#include "../PS_Base/PS_MathBase.h"

#ifdef PS_OS_WINDOWS
	#include <CL/cl.h>
	#include <CL/cl_gl.h>
	#include <CL/cl_ext.h>
#elif defined(PS_OS_LINUX)
	#include <CL/opencl.h>
	#include <GL/glxew.h>
#elif defined(PS_OS_MAC)
//#include <open>
#endif


using namespace std;

namespace PS{
namespace HPC{

/*!s
  * A Kernel is a simple function to run on device. Kernel runs can communicate through share memory on compute device only.
  * The amount of memory available to a kernel function is limited. 
  */
class ComputeKernel
{
public:
    ComputeKernel(cl_kernel kernel, const string& strTitle)
    {
        m_clKernel = kernel;
        m_strTitle = strTitle;
    }

    virtual ~ComputeKernel() {
        clReleaseKernel(m_clKernel);
    }

    /*!
     * sets kernel argument at the specified index position.
     * @param idxArg index position of the argument
     * @param size Size of the argument
     * @lpArg pointer to the beginning of the argument to set.
     */
    bool setArg(U32 idxArg, U32 size, const void* lpArg);
    cl_kernel getKernel() const {return m_clKernel;}
    std::string getTitle() const {return m_strTitle;}

    size_t getKernelWorkGroupSize() const {return m_szWorkGroupSize;}
    void setKernelWorkGroupSize(size_t szWorkgroup) { m_szWorkGroupSize = szWorkgroup;}
private:
   size_t m_szWorkGroupSize;
   cl_kernel m_clKernel;
   std::string m_strTitle;
};

/*!
  * A program can contain multiple kernels.
  */
class ComputeProgram
{
public:
    ComputeProgram(cl_program program)
    {
        m_clProgram = program;
    }
    virtual ~ComputeProgram();

	bool saveBinary(const char* chrBinFilePath);
    ComputeKernel* addKernel(const char* chrKernelTitle);
private:
    cl_program m_clProgram;
    std::vector<ComputeKernel*> m_lstKernels;
};

/*!
  * A compute device represents and OpenCL Compute Device and can hold multiple programs.
  * An OpenCL program can be added directly from its source code or from a source file.
  * Once the program is added the binary file can be stored.
  * Compute Device can create memory buffer objects for the kernels.
  */
class ComputeDevice{
public:
    enum DEVICETYPE{dtCPU = CL_DEVICE_TYPE_CPU, dtGPU = CL_DEVICE_TYPE_GPU};
    enum MEMACCESSMODE{memReadWrite = CL_MEM_READ_WRITE, memReadOnly = CL_MEM_READ_ONLY, memWriteOnly = CL_MEM_WRITE_ONLY};

	//Constructors
    ComputeDevice();
    ComputeDevice(DEVICETYPE dev, bool bWithOpenGLInterOp = true, const char* lpPlatformProvide = "DEFAULT");
  
	//Destructor
	virtual ~ComputeDevice();


	bool isBinarySaved() const { return m_bSaveBinary;}
	void setSaveBinary(bool bSave) 
	{
		m_bSaveBinary = bSave;
	}


    bool isReady() const {return m_bReady;}

    /*!
     * Adds a new ComputeProgram from inline source code.
     * @param chrComputeCode inline source code
     * @return pointer to the ComputeProgram object created.
     */
    ComputeProgram* addProgram(const char* chrComputeCode);

    /*!
     * Adds a new ComputeProgram from source on disc.
     * @param chrFilePath path to the OpenCL file on disc.
     * @return pointer to the ComputeProgram object created.
     */
    ComputeProgram* addProgramFromFile(const char* chrFilePath);

    //Remove program
    void removeProgram(const ComputeProgram* lpProgram);

    /*!
     * Prints device info to default output screen.
     */
    void printInfo();

    /*!
     * Create memory buffer for readonly or writeonly access.
     */
    cl_mem createMemBuffer(const size_t size, MEMACCESSMODE mode);

    /*!
     * Create memory buffer for readonly or writeonly access from a GL Buffer.
     */
    cl_mem createMemBufferFromGL(cl_GLuint glBuffer, MEMACCESSMODE mode);

    /*!
     * Enqueues a write operation in the command queue
     */
    bool enqueueWriteBuffer(cl_mem destMem, U32 size, const void* lpSource);

    /*!
     * Enqueues a read operation in the command queue
     */
    bool enqueueReadBuffer(cl_mem srcMem, U32 size, void* lpDest);

    /*!
     * Wait to finish all commands in the command q
     */
    void finishAllCommands();


	size_t getKernelWorkgroupSize(ComputeKernel* lpKernel);

    //Access
    cl_device_id getDevice() const {return m_clDeviceID;}
    cl_context getContext() const {return m_clContext;}
    cl_command_queue getCommandQ() const {return m_clCommandQueue;}

private:
	/*!
	* @param dev Device type for running the compute kernels on
	* @param bWithOpenGLInterOp using openGL inter-operability
	* @param lpStrPlatformProvider the string name of the platform to use
	*/
	bool initDevice(DEVICETYPE dev, bool bWithOpenGLInterOp, const char* lpStrPlatformProvider);

public:
    static cl_int oclPlatformID(cl_platform_id* clSelectedPlatformID, const char* lpStrProvider = "NVIDIA");
    //static void oclPrintDevInfo(int iLogMode, cl_device_id device);


    /*! Get and return device capability
     * @return the 2 digit integer representation of device Cap (major minor). return -1 if NA
     * @param device         OpenCL id of the device
     */
    static int oclGetDevCap(cl_device_id device);

    /*! Gets the id of the nth device from the context
     * @return the id or -1 when out of range
     * @param cxGPUContext         OpenCL context
     * @param device_idx            index of the device of interest
     */
    static cl_device_id oclGetDev(cl_context cxGPUContext, unsigned int device_idx);

    /*! Gets the id of device with maximal FLOPS from the context
     * @return the id
     * @param cxGPUContext         OpenCL context
     */
    static cl_device_id oclGetMaxFlopsDev(cl_context cxGPUContext);


    /*! Get the binary (PTX) of the program associated with the device
     * @param cpProgram    OpenCL program
     * @param cdDevice     device of interest
     * @param binary       returned code
     * @param length       length of returned code
     */
    static void oclGetProgBinary( cl_program cpProgram, cl_device_id cdDevice, char** binary, size_t* length);

    /*! Get and log the binary (PTX) from the OpenCL compiler for the requested program & device
     * @param cpProgram    OpenCL program
     * @param cdDevice     device of interest
     * @param const char*  cPtxFileName   optional PTX file name
     */
    static void oclLogPtx(cl_program cpProgram, cl_device_id cdDevice, const char* cPtxFileName);

    /*!
     * Returns a string representing the image format
     * @param uiImageFormat the code representing the image format
     */
    static const char* oclImageFormatString(cl_uint uiImageFormat);

    /*!
     * Return an error string regarding a returned error.
     * @param error integer code of the error
     */
    static const char* oclErrorString(cl_int error);

private:
    DEVICETYPE      m_deviceType;
    cl_platform_id  m_clPlatform;
    cl_device_id    m_clDeviceID;

    cl_context      m_clContext;
    cl_command_queue m_clCommandQueue;

    std::vector<ComputeProgram*> m_lstPrograms;

	bool m_bSaveBinary;
    bool m_bReady;
};


}
}

#endif // PS_COMPUTEDEVICE_H
