// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_DOCKER_THREAD_H
#define XENGINE_DOCKER_THREAD_H

#include <boost/function.hpp>
#include <boost/thread/thread.hpp>
#include <string>

namespace xengine
{

class CDock;

class CThread
{
    friend class CDocker;

public:
    typedef boost::function<void()> ThreadFunc;
    CThread(const std::string& strNameIn, ThreadFunc fnCallbackIn)
      : strThreadName(strNameIn), pThread(NULL), fnCallback(fnCallbackIn), fRunning(false)
    {
    }

    virtual ~CThread()
    {
        delete pThread;
    }

    bool IsRunning()
    {
        return fRunning;
    }

    void Interrupt()
    {
        if (pThread)
        {
            pThread->interrupt();
        }
    }

    void Exit()
    {
        if (pThread)
        {
            pThread->join();
            delete pThread;
            pThread = NULL;
        }
    }

protected:
    const std::string strThreadName;
    boost::thread* pThread;
    ThreadFunc fnCallback;
    bool fRunning;
};

} // namespace xengine

#endif //XENGINE_DOCKER_THREAD_H
