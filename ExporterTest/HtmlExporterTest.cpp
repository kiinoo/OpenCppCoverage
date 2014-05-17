#include "stdafx.h"

#include "CppCoverage/CoverageData.hpp"
#include "CppCoverage/ModuleCoverage.hpp"
#include "CppCoverage/FileCoverage.hpp"
#include "Tools/TemporaryPath.hpp"

#include <boost/filesystem.hpp>

#include "Exporter/Html/HtmlExporter.hpp"
#include "Exporter/Html/HtmlFolderStructure.hpp"

namespace cov = CppCoverage;
namespace fs = boost::filesystem;

namespace ExporterTest
{
	//-------------------------------------------------------------------------
	struct HtmlExporterTest : public ::testing::Test
	{
		//-------------------------------------------------------------------------
		HtmlExporterTest()
			: htmlExporter_{fs::canonical("../Exporter/Html/Template") }
		{

		}

		//---------------------------------------------------------------------
		bool Contains(std::wistream& istr, const std::wstring& str)
		{			
			std::wstring line;

			while (std::getline(istr, line))
			{
				if (line.find(str) != std::string::npos)
					return true;
			}

			return false;
		}

		//---------------------------------------------------------------------
		void CheckWarningInIndex(bool expectedValue)
		{
			auto indexPath = output_.GetPath() / "index.html";
			ASSERT_TRUE(fs::exists(indexPath));
			std::wifstream ifs{ indexPath.string()};
			bool hasWarning = Contains(ifs, Exporter::HtmlExporter::WarningExitCodeMessage);
			
			ASSERT_EQ(expectedValue, hasWarning);
		}

		Exporter::HtmlExporter htmlExporter_;
		Tools::TemporaryPath output_;
	};	

	//-------------------------------------------------------------------------
	TEST_F(HtmlExporterTest, Export)
	{	
		fs::path testFolder = fs::canonical("../ExporterTest/Data");	
		cov::CoverageData data{ L"Test", 0};

		auto& module1 = data.AddModule(L"Module1.exe");
		auto& file1 = module1.AddFile(testFolder / L"TestFile1.cpp");
		auto& file2 = module1.AddFile(testFolder / L"TestFile2.cpp");
		
		file1.AddLine(0, true);
		file2.AddLine(0, true);
		
		data.AddModule(L"Module2.exe").AddFile(testFolder / L"TestFile1.cpp").AddLine(0,true);
		data.AddModule(L"Module3.exe").AddFile(testFolder / L"TestFile1.cpp").AddLine(0, true);
		data.ComputeCoverageRate();

		htmlExporter_.Export(data, output_);

		auto modulesPath = output_.GetPath() / Exporter::HtmlFolderStructure::FolderModules;
		ASSERT_TRUE(fs::exists(output_.GetPath() / "index.html"));
		ASSERT_TRUE(fs::exists(modulesPath / "module1.html"));
		ASSERT_FALSE(fs::exists(modulesPath / "module2.html"));
		ASSERT_TRUE(fs::exists(modulesPath / "module1" / "TestFile1.html"));
		ASSERT_TRUE(fs::exists(modulesPath / "module1" / "TestFile2.html"));
	}

	//-------------------------------------------------------------------------
	TEST_F(HtmlExporterTest, NoWarning)
	{
		cov::CoverageData data{ L"Test", 0 };

		htmlExporter_.Export(data, output_);		
		CheckWarningInIndex(false);
	}

	//-------------------------------------------------------------------------
	TEST_F(HtmlExporterTest, Warning)
	{
		cov::CoverageData data{ L"Test", 42};

		htmlExporter_.Export(data, output_);	
		CheckWarningInIndex(true);
	}
}

