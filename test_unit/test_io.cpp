/** ==========================================================================
* 2011 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
* ============================================================================*/

#include <gtest/gtest.h>
#include "g2log.h"
#include "g2logworker.h"
#include <memory>
#include <string>
#include <fstream>
#include <cstdio>

namespace
{
   const std::string log_directory = "./";

   bool verifyContent(const std::string &total_text,std::string msg_to_find)
   {
      std::string content(total_text);
      size_t location = content.find(msg_to_find);
      return (location != std::string::npos);
   }

	
   std::string readFileToText(std::string filename)
   {
      std::ifstream in;
      in.open(filename.c_str(),std::ios_base::in);
      if(!in.is_open())
      {
         return ""; // error just return empty string - test will 'fault'
      }
      std::ostringstream oss;
      oss << in.rdbuf();
      std::string content(oss.str());
      return content;
   }

    g2::internal::FatalMessage g_latest_fatal_message = {"dummy", g2::internal::FatalMessage::kReasonFatal, -1};
    void MockFatalCall (g2::internal::FatalMessage fatalMsg) {
       g_latest_fatal_message = fatalMsg;
    }


} // end anonymous namespace






// RAII temporarily replace of logger
// and restoration of original logger at scope end
struct RestoreLogger
{
   RestoreLogger();
   ~RestoreLogger();
   void reset();

   std::unique_ptr<g2LogWorker> logger_;
   std::string logFile(){return log_file_;}
private:
   std::string log_file_;
};

RestoreLogger::RestoreLogger()
	: logger_(new g2LogWorker("UNIT_TEST_LOGGER", log_directory))
{
        g_latest_fatal_message.message_ = {""};
	g2::initializeLogging(logger_.get());
	g2::internal::changeFatalInitHandlerForUnitTesting( MockFatalCall );

	std::future<std::string> filename(logger_->logFileName());
	EXPECT_TRUE(filename.valid());
	log_file_ = filename.get();
}

RestoreLogger::~RestoreLogger()
{
	reset();
	g2::shutDownLogging();
	EXPECT_EQ(0, remove(log_file_.c_str()));
}

void RestoreLogger::reset()
{
	logger_.reset();
}



// LOG
TEST(Initialization, No_Logger_Initialized___Expecting_LOG_calls_to_be_Still_OK) 
{
   std::string err_msg1 = "Hey. I am not instantiated but I still should not crash. (I am g2logger)";
   std::string err_msg2_ignored = "This uninitialized message should be ignored";
   try {
      LOG(INFO) << err_msg1;
      LOG(INFO) << err_msg2_ignored;

   } catch (std::exception& e) {
      ADD_FAILURE() << "Should never have thrown even if it is not instantiated";
   }

   RestoreLogger logger;
   std::string good_msg1 = "This message will pull in also the uninitialized_call message";
   LOG(INFO) << good_msg1;
   logger.reset();
   auto content =  readFileToText(logger.logFile());
   ASSERT_TRUE(verifyContent(content, err_msg1)) << "Content: [" << content << "]";
   ASSERT_FALSE(verifyContent(content, err_msg2_ignored)) << "Content: [" << content << "]";
   ASSERT_TRUE(verifyContent(content, good_msg1)) << "Content: [" << content << "]";
}


namespace  { 
   const std::string t_info = "test INFO ";
   const std::string t_info2 = "test INFO 123";
   const std::string t_debug = "test DEBUG ";
   const std::string t_debug2 = "test DEBUG 1.123456";
   const std::string t_warning = "test WARNING ";
   const std::string t_warning2 = "test WARNING yello";
}


TEST(CompileTest, LogWithIf) {
  std::string content;
  {
     RestoreLogger logger;

     if( !t_info.empty()) 
       LOGF(INFO, "Hello 1");
     else 
       LOGF(INFO, "Bye 1");
     

     if(t_info.empty())
       LOG(INFO) << "Hello 2";
     else
       LOG(INFO) << "Bye 2";
    
     logger.reset();
     content = readFileToText(logger.logFile());
   }
   EXPECT_TRUE(verifyContent(content, "Hello 1"));
   EXPECT_FALSE(verifyContent(content, "Bye 1"));
   EXPECT_FALSE(verifyContent(content, "Hello 2"));
   EXPECT_TRUE(verifyContent(content, "Bye 2"));
}

 
TEST(Basics, Shutdown) {
   std::string file_content;
   {
      RestoreLogger logger{};
      LOG(INFO) << "Not yet shutdown. This message should make it";
      logger.reset(); // force flush of logger (which will trigger a shutdown)
      LOG(INFO) << "Logger is shutdown,. this message will not make it (but it's safe to try)";
      file_content = readFileToText(logger.logFile()); // logger is already reset
      SCOPED_TRACE("LOG_INFO"); // Scope exit be prepared for destructor failure
   }
   EXPECT_TRUE(verifyContent(file_content, "Not yet shutdown. This message should make it"));
   EXPECT_FALSE(verifyContent(file_content, "Logger is shutdown,. this message will not make it (but it's safe to try)"));
}

TEST(Basics, Shutdownx2) {
   std::string file_content;
   {
      RestoreLogger logger{};
      LOG(INFO) << "Not yet shutdown. This message should make it";
      logger.reset(); // force flush of logger (which will trigger a shutdown)
      g2::shutDownLogging(); // already called in reset, but safe to call again
      LOG(INFO) << "Logger is shutdown,. this message will not make it (but it's safe to try)";
      logger.reset();
      file_content = readFileToText(logger.logFile()); // already reset
      SCOPED_TRACE("LOG_INFO"); // Scope exit be prepared for destructor failure
   }
   EXPECT_TRUE(verifyContent(file_content, "Not yet shutdown. This message should make it"));
   EXPECT_FALSE(verifyContent(file_content, "Logger is shutdown,. this message will not make it (but it's safe to try)"));
}

TEST(Basics, ShutdownActiveLogger) {
   std::string file_content;
   {
      RestoreLogger logger{};
      LOG(INFO) << "Not yet shutdown. This message should make it";
      EXPECT_TRUE(g2::shutDownLoggingForActiveOnly(logger.logger_.get()));
      LOG(INFO) << "Logger is shutdown,. this message will not make it (but it's safe to try)";
      logger.reset();
      file_content = readFileToText(logger.logFile());
      SCOPED_TRACE("LOG_INFO"); // Scope exit be prepared for destructor failure
   }
   EXPECT_TRUE(verifyContent(file_content, "Not yet shutdown. This message should make it")) << "\n\n\n***************************\n" << file_content;
   EXPECT_FALSE(verifyContent(file_content, "Logger is shutdown,. this message will not make it (but it's safe to try)"));
}

TEST(Basics, DoNotShutdownActiveLogger) {
   std::string file_content;
   std::string toRemove;
   {
      RestoreLogger logger{};
      LOG(INFO) << "Not yet shutdown. This message should make it";
      g2LogWorker duplicateLogWorker{{"test_duplicate"},{"./"}};
      toRemove = duplicateLogWorker.logFileName().get();

      EXPECT_FALSE(g2::shutDownLoggingForActiveOnly(&duplicateLogWorker));
      LOG(INFO) << "Logger is (NOT) shutdown,. this message WILL make it";
      logger.reset();
      file_content = readFileToText(logger.logFile());
      SCOPED_TRACE("LOG_INFO"); // Scope exit be prepared for destructor failure
   }
   EXPECT_EQ(0, remove(toRemove.c_str()));

   EXPECT_TRUE(verifyContent(file_content, "Not yet shutdown. This message should make it"));
   EXPECT_TRUE(verifyContent(file_content, "Logger is (NOT) shutdown,. this message WILL make it")) << file_content;
}


// printf-type log
TEST(LogTest, LOG_F)
{
   std::string file_content;
   {
	RestoreLogger logger;
	//std::cout << "logfilename: " << logger.logFile() << std::flush << std::endl;
	LOGF(INFO, std::string(t_info + "%d").c_str(), 123);
	LOGF(DEBUG, std::string(t_debug + "%f").c_str(), 1.123456);
	LOGF(WARNING, std::string(t_warning + "%s").c_str(), "yello");
	logger.reset(); // force flush of logger
	file_content = readFileToText(logger.logFile());
	SCOPED_TRACE("LOG_INFO");  // Scope exit be prepared for destructor failure
   }
   ASSERT_TRUE(verifyContent(file_content, t_info2));
   ASSERT_TRUE(verifyContent(file_content, t_debug2));
   ASSERT_TRUE(verifyContent(file_content, t_warning2));
}




// stream-type log
TEST(LogTest, LOG)
{
	std::string file_content;
	{
		RestoreLogger logger;
		LOG(INFO) << t_info   << 123;
		LOG(DEBUG) <<  t_debug  << std::setprecision(7) << 1.123456f;
		LOG(WARNING) << t_warning << "yello";
		logger.reset(); // force flush of logger
		file_content = readFileToText(logger.logFile());
		SCOPED_TRACE("LOG_INFO");  // Scope exit be prepared for destructor failure
	}
	ASSERT_TRUE(verifyContent(file_content, t_info2));
	ASSERT_TRUE(verifyContent(file_content, t_debug2));
	ASSERT_TRUE(verifyContent(file_content, t_warning2));
}


TEST(LogTest, LOG_F_IF)
{
	std::string file_content;
	{
		RestoreLogger logger;
		LOGF_IF(INFO, (2 == 2), std::string(t_info + "%d").c_str(), 123);
		LOGF_IF(DEBUG, (2 != 2), std::string(t_debug + "%f").c_str(), 1.123456);
		logger.reset(); // force flush of logger
		file_content = readFileToText(logger.logFile());
		SCOPED_TRACE("LOG_IF");  // Scope exit be prepared for destructor failure
	}
	ASSERT_TRUE(verifyContent(file_content, t_info2));
	ASSERT_FALSE(verifyContent(file_content, t_debug2));
}

TEST(LogTest, LOG_IF)
{
	std::string file_content;
	{
		RestoreLogger logger;
		LOG_IF(INFO, (2 == 2))  << t_info   << 123;
		LOG_IF(DEBUG, (2 != 2)) <<  t_debug  << std::setprecision(7) << 1.123456f;
		logger.reset(); // force flush of logger
		file_content = readFileToText(logger.logFile());
		SCOPED_TRACE("LOG_IF");  // Scope exit be prepared for destructor failure
	}
	ASSERT_TRUE(verifyContent(file_content, t_info2));
	ASSERT_FALSE(verifyContent(file_content, t_debug2));
}

TEST(LogTest, LOGF__FATAL)
{
	RestoreLogger logger;
        LOGF(FATAL, "This message is fatal %d",0);
        logger.reset();
         if(verifyContent(g_latest_fatal_message.message_, "EXIT trigger caused by ") &&
			verifyContent(g_latest_fatal_message.message_, "FATAL") &&
			verifyContent(g_latest_fatal_message.message_, "This message is fatal"))
		{
			SUCCEED();
			return;
		}
		else
		{
			ADD_FAILURE() << "Didn't get the expected FATAL call";
		}
}


TEST(LogTest, LOG_FATAL)
{
	RestoreLogger logger;
        ASSERT_EQ(g_latest_fatal_message.message_, "");
        LOG(FATAL) << "This message is fatal";
	if(verifyContent(g_latest_fatal_message.message_, "EXIT trigger caused by ") &&
	   verifyContent(g_latest_fatal_message.message_, "FATAL") &&
	    verifyContent(g_latest_fatal_message.message_, "This message is fatal"))
	{
		SUCCEED();
		return;
	}
	else
	{
		ADD_FAILURE() << "Did not get fatal call as expected";
	}
}


TEST(LogTest, LOGF_IF__FATAL)
{
	RestoreLogger logger;
        LOGF_IF(FATAL, (2<3), "This message%sis fatal"," ");
        logger.reset();
	std::string file_content = readFileToText(logger.logFile());
	if(verifyContent(g_latest_fatal_message.message_, "EXIT trigger caused by ") &&
           verifyContent(g_latest_fatal_message.message_, "FATAL") &&
	   verifyContent(g_latest_fatal_message.message_, "This message is fatal"))
	{
	   SUCCEED();
	   return;
	}
	else
	{
	   ADD_FAILURE() << "Did not get fatal call as expected";
	}

}


TEST(LogTest, LOG_IF__FATAL)
{
	RestoreLogger logger;
        ASSERT_EQ(g_latest_fatal_message.message_, "");
        LOG_IF(WARNING, (0 != t_info.compare(t_info))) << "This message should NOT be written";
	LOG_IF(FATAL, (0 != t_info.compare(t_info2))) << "This message is fatal";
        logger.reset();
	if(verifyContent(g_latest_fatal_message.message_, "EXIT trigger caused by ") &&
		verifyContent(g_latest_fatal_message.message_, "FATAL") &&
		verifyContent(g_latest_fatal_message.message_, "This message is fatal") &&
		(false == verifyContent(g_latest_fatal_message.message_, "This message should NOT be written")))
	{
		SUCCEED();
		return;
	}
	else
	{
		ADD_FAILURE() << "Did not get fatal call as expected";
	}

}

TEST(LogTest, LOG_IF__FATAL__NO_THROW)
{
	RestoreLogger logger;
        ASSERT_EQ(g_latest_fatal_message.message_, "");
        LOG_IF(FATAL, (2>3)) << "This message%sshould NOT throw";
	ASSERT_EQ(g_latest_fatal_message.message_, "");
}


// CHECK_F
TEST(CheckTest, CHECK_F__thisWILL_PrintErrorMsg)
{
	RestoreLogger logger;
        ASSERT_EQ(g_latest_fatal_message.message_, "");
	CHECK(1 == 2);
	logger.reset();
	if(verifyContent(g_latest_fatal_message.message_, "EXIT trigger caused by ") &&
		verifyContent(g_latest_fatal_message.message_, "FATAL"))
	{
		SUCCEED();
		return;
	}
	else
	{
		ADD_FAILURE() << "Did not get fatal call as expected";
	}
}


TEST(CHECK_F_Test, CHECK_F__thisWILL_PrintErrorMsg)
{
	RestoreLogger logger;
	std::string msg = "This message is added to throw %s and %s";
	std::string msg2 = "This message is added to throw message and log";
	std::string arg1 = "message";
	std::string arg2 = "log";
        CHECK_F(1 >= 2, msg.c_str(), arg1.c_str(), arg2.c_str());
	logger.reset();
	if(verifyContent(g_latest_fatal_message.message_, "EXIT trigger caused by ") &&
	   verifyContent(g_latest_fatal_message.message_, "FATAL") &&
	   verifyContent(g_latest_fatal_message.message_ , msg2))
	{
	   SUCCEED();
	   return;
	}
	ADD_FAILURE() << "Did not get fatal call as expected";
}

TEST(CHECK_Test, CHECK__thisWILL_PrintErrorMsg)
{
	RestoreLogger logger;
	std::string msg = "This message is added to throw %s and %s";
	std::string msg2 = "This message is added to throw message and log";
	std::string arg1 = "message";
	std::string arg2 = "log";
        CHECK(1 >= 2) << msg2;
	logger.reset();
	if(verifyContent(g_latest_fatal_message.message_, "EXIT trigger caused by ") &&
	   verifyContent(g_latest_fatal_message.message_, "FATAL") &&
	   verifyContent(g_latest_fatal_message.message_, msg2))
	{
	   SUCCEED();
	   return;
	}
	
	ADD_FAILURE() << "Did not get fatal call as expected";
}


TEST(CHECK, CHECK_ThatWontThrow)
{
	RestoreLogger logger;
	std::string msg = "This %s should never appear in the %s";
	std::string msg2 = "This message should never appear in the log";
	std::string arg1 = "message";
	std::string arg2 = "log";
        CHECK(1 == 1);
	CHECK_F(1==1, msg.c_str(), "message", "log");
	ASSERT_FALSE(verifyContent(g_latest_fatal_message.message_, msg2));
	ASSERT_EQ(g_latest_fatal_message.message_, ""); // just to be obvious
}

