/*
	Copyright (C) 2009-2021 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>

#include <condition_variable>
#include <mutex>
#include <thread>

#include "types.h"
#include "task.h"

class Task::Impl {
public:
	std::thread _thread;
	bool _isThreadRunning;
	std::mutex _mutex;
	std::condition_variable _condWork;

	Impl();
	~Impl();

	void start(bool spinlock, int threadPriority, const char *name);
	void execute(const TWork &work, void *param);
	void* finish();
	void shutdown();

	TWork workFunc;
	void *workFuncParam;
	void *ret;
	bool exitThread;
};

static void taskProc(Task::Impl *ctx)
{
	do {
		std::unique_lock<std::mutex> lock(ctx->_mutex);
		ctx->_condWork.wait(lock, [&]() { return ctx->workFunc != NULL || ctx->exitThread; });

		if (ctx->workFunc != NULL) {
			ctx->ret = ctx->workFunc(ctx->workFuncParam);
		} else {
			ctx->ret = NULL;
		}

		ctx->workFunc = NULL;
		ctx->_condWork.notify_all();
	} while(!ctx->exitThread);
}

Task::Impl::Impl()
{
	_isThreadRunning = false;
	workFunc = NULL;
	workFuncParam = NULL;
	ret = NULL;
	exitThread = false;
}

Task::Impl::~Impl()
{
	shutdown();
}

void Task::Impl::start(bool spinlock, int threadPriority, const char *name)
{
	(void)spinlock;
	(void)threadPriority;
	(void)name;
	std::lock_guard<std::mutex> lock(this->_mutex);
	if (this->_isThreadRunning) {
		return;
	}

	this->workFunc = NULL;
	this->workFuncParam = NULL;
	this->ret = NULL;
	this->exitThread = false;
	this->_thread = std::thread(taskProc, this);
	this->_isThreadRunning = true;
}

void Task::Impl::execute(const TWork &work, void *param)
{
	std::lock_guard<std::mutex> lock(this->_mutex);
	if ((work == NULL) || (this->workFunc != NULL) || !this->_isThreadRunning)
	{
		return;
	}

	this->workFunc = work;
	this->workFuncParam = param;
	this->_condWork.notify_all();
}

void* Task::Impl::finish()
{
	std::unique_lock<std::mutex> lock(this->_mutex);
	if ((this->workFunc == NULL) || !this->_isThreadRunning) {
		return NULL;
	}

	this->_condWork.wait(lock, [&]() { return this->workFunc == NULL; });
	return this->ret;
}

void Task::Impl::shutdown()
{
	{
		std::lock_guard<std::mutex> lock(this->_mutex);
		if (!this->_isThreadRunning) {
			return;
		}

		this->workFunc = NULL;
		this->exitThread = true;
		this->_condWork.notify_all();
	}

	if (this->_thread.joinable()) {
		this->_thread.join();
	}

	std::lock_guard<std::mutex> lock(this->_mutex);
	this->_isThreadRunning = false;
}

void Task::start(bool spinlock) { impl->start(spinlock, 0, NULL); }
void Task::start(bool spinlock, int threadPriority, const char *name) { impl->start(spinlock, threadPriority, name); }
void Task::shutdown() { impl->shutdown(); }
Task::Task() : impl(new Task::Impl()) {}
Task::~Task() { delete impl; }
void Task::execute(const TWork &work, void* param) { impl->execute(work,param); }
void* Task::finish() { return impl->finish(); }
