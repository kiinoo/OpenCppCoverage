// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2014 OpenCppCoverage
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "stdafx.h"
#include "ExecutedAddressManager.hpp"

#include <unordered_map>
#include <boost/container/small_vector.hpp>

#include "tools/Log.hpp"

#include "CppCoverageException.hpp"
#include "ModuleCoverage.hpp"
#include "FileCoverage.hpp"
#include "Address.hpp"

namespace CppCoverage
{
	//-------------------------------------------------------------------------
	struct ExecutedAddressManager::Line
	{
		explicit Line(unsigned char instructionToRestore)
		: instructionToRestore_{ instructionToRestore }
		{
		}

		unsigned char instructionToRestore_;
		boost::container::small_vector<bool*, 1> hasBeenExecutedCollection_;
	};

	//-------------------------------------------------------------------------
	struct ExecutedAddressManager::File
	{		
		// Use map to have iterator always valid
		std::map<unsigned int, bool> lines;
	};

	//-------------------------------------------------------------------------
	struct ExecutedAddressManager::Module
	{
		explicit Module(const std::wstring& name)
		: name_(name)
		{
		}

		std::wstring name_;
		std::unordered_map<std::wstring, File> files_;
	};
	
	//-------------------------------------------------------------------------
	ExecutedAddressManager::ExecutedAddressManager()
		: lastModule_{nullptr}
	{
	}

	//-------------------------------------------------------------------------
	ExecutedAddressManager::~ExecutedAddressManager()
	{
	}

	//-------------------------------------------------------------------------
	void ExecutedAddressManager::AddModule(const std::wstring& moduleName)
	{
		auto it = modules_.find(moduleName);

		if (it == modules_.end())
			it = modules_.emplace(moduleName, Module{ moduleName }).first;
		lastModule_ = &it->second;
	}
	
	//-------------------------------------------------------------------------
	bool ExecutedAddressManager::RegisterAddress(
		const Address& address,
		const std::wstring& filename,
		unsigned int lineNumber, 
		unsigned char instructionValue)
	{
		auto& module = GetLastAddedModule();
		auto& file = module.files_[filename];
		auto& lines = file.lines;

		LOG_TRACE << "RegisterAddress: " << address << " for " << filename << ":" << lineNumber;

		// Different {filename, line} can have the same address.
		// Same {filename, line} can have several addresses.		
		bool keepBreakpoint = false;
		auto itAddress = addressLineMap_.find(address);

		if (itAddress == addressLineMap_.end())
		{
			itAddress = addressLineMap_.emplace(address, Line{ instructionValue }).first;
			keepBreakpoint = true;
		}
		
		auto& line = itAddress->second;
		line.hasBeenExecutedCollection_.push_back(&lines[lineNumber]);
		
		return keepBreakpoint;
	}

	//-------------------------------------------------------------------------
	ExecutedAddressManager::Module& ExecutedAddressManager::GetLastAddedModule()
	{
		if (!lastModule_)
			THROW("Cannot get last module.");

		return *lastModule_;
	}

	//-------------------------------------------------------------------------
	boost::optional<unsigned char> ExecutedAddressManager::MarkAddressAsExecuted(
		const Address& address)
	{
		auto it = addressLineMap_.find(address);

		if (it == addressLineMap_.end())
			return boost::none;

		auto& line = it->second;

		for (bool* hasBeenExecuted : line.hasBeenExecutedCollection_)
		{
			if (!hasBeenExecuted)
				THROW("Invalid pointer");
			*hasBeenExecuted = true;
		}
		return line.instructionToRestore_;
	}
	
	//-------------------------------------------------------------------------
	CoverageData ExecutedAddressManager::CreateCoverageData(
		const std::wstring& name,
		int exitCode) const
	{
		CoverageData coverageData{ name, exitCode };

		for (const auto& pair : modules_)
		{
			const auto& module = pair.second;
			auto& moduleCoverage = coverageData.AddModule(module.name_);

			for (const auto& file : module.files_)
			{
				const std::wstring& name = file.first;
				const File& fileData = file.second;

				auto& fileCoverage = moduleCoverage.AddFile(name);

				for (const auto& pair : fileData.lines)
				{
					auto lineNumber = pair.first;
					bool hasLineBeenExecuted = pair.second;
					
					fileCoverage.AddLine(lineNumber, hasLineBeenExecuted);
				}
			}			
		}

		return coverageData;
	}

	//-------------------------------------------------------------------------
	void ExecutedAddressManager::OnExitProcess(HANDLE hProcess)
	{
		auto it = addressLineMap_.begin();

		while (it != addressLineMap_.end())
		{
			const Address& address = it->first;

			if (address.GetProcessHandle() == hProcess)
				it = addressLineMap_.erase(it);
			else
				++it;
		}
	}
}
