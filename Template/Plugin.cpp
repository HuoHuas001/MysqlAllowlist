#include "pch.h"
#include <EventAPI.h>
#include <LoggerAPI.h>
#include <MC/Level.hpp>
#include <MC/BlockInstance.hpp>
#include <MC/Block.hpp>
#include <MC/BlockSource.hpp>
#include <MC/Actor.hpp>
#include <MC/Player.hpp>
#include <MC/ItemStack.hpp>
#include <LLAPI.h>
#include "../include/MySQL/MySQL/mysql.h"
#include <Nlohmann/json.hpp>
#include <PlayerInfoAPI.h>
#include <RegCommandAPI.h>

Logger logger("MySQLAllowList");
//MySQL
MYSQL* con;
MYSQL_RES* res;
MYSQL_ROW row;
//database configuartion
string user = "root";
string pswd = "123456";
string host = "localhost";
string table = "hospital";
unsigned port = 3306;

using json = nlohmann::json;

bool getFile() {
	try {
		json j;			// 创建 json 对象
		std::ifstream jfile("plugins\\AllowList\\mysql.json");
		if (jfile) {
			jfile >> j;		// 以文件流形式读取 json 文件
			host = (string)j["host"].get<string>();
			user = (string)j["user"].get<string>();
			pswd = (string)j["password"].get<string>();
			table = (string)j["table"].get<string>();
			port = (int)j["port"].get<int>();
			return true;
		}
	}
	catch (...) {
		return false;
	}
	return false;
}

void disconnectdb() {
	mysql_free_result(res);
	mysql_close(con);
}

bool initDB() {
	getFile();
	try {
		mysql_library_init(0, NULL, NULL);//初始化MySQL库  
		con = mysql_init((MYSQL*)0);
		bool sqlConStatus = mysql_real_connect(con, host.c_str(), user.c_str(), pswd.c_str(), table.c_str(), port, NULL, 0);
		if (sqlConStatus) {
			const char* query = "set names \'UTF8\'";
			mysql_real_query(con, query, strlen(query));
			mysql_query(con, "PRAGMA journal_mode = MEMORY");
			mysql_query(con, "PRAGMA synchronous = NORMAL");
			mysql_query(con, "CREATE TABLE IF NOT EXISTS allowlist(XUID TEXT NOT NULL,NAME TEXT NOT NULL,); ");
		}
		else {
			logger.error("DB err {}", mysql_error(con));
			return false;
		}

	}
	catch (std::exception const& e) {
		logger.error("DB err {}", e.what());
		return false;
	}
	return true;
}

bool queryMysqlPlayerIn(string name,string xuid) {
	initDB();
	char queryC[400];
	sprintf(queryC, "SELECT * FROM `allowlist` WHERE name='%s'", name.c_str());
	auto rt = mysql_real_query(con, queryC, strlen(queryC));
	if (rt)
	{
		logger.error("DB err {}", mysql_error(con));
		return false;
	}

	res = mysql_store_result(con);//将结果保存在res结构体中
	if (!res) {
		logger.error("DB err {}", mysql_error(con));
		return false;
	}
	string rv = "";
	string nv = "";
	bool fg = false;
	while (auto row = mysql_fetch_row(res)) {
		rv = row[0];
		nv = row[1];
		if (name == nv) {
			if (rv == xuid) {
				fg = true;
			}
			else if (rv == "") {
				char queryC[400];
				sprintf(queryC, "UPDATE `allowlist` SET `XUID`='%s' WHERE NAME='%s'", xuid.c_str(), name.c_str());
				mysql_real_query(con, queryC, strlen(queryC));
				fg = true;
			}
		}
	}
	mysql_free_result(res);
	mysql_close(con);
	return fg;
}

string queryMysqlPlayerList() {
	initDB();
	char queryC[400];
	sprintf(queryC, "SELECT * FROM `allowlist`");
	auto rt = mysql_real_query(con, queryC, strlen(queryC));
	if (rt)
	{
		logger.error("DB err {}", mysql_error(con));
		return "";
	}

	res = mysql_store_result(con);//将结果保存在res结构体中
	if (!res) {
		logger.error("DB err {}", mysql_error(con));
		return "";
	}
	string rv = "";
	string xv = "";
	string rl = "";
	bool fg = false;
	while (auto row = mysql_fetch_row(res)) {
		xv = row[0];
		rv = row[1];
		string pl = "{\"name\":\""+rv+"\",\"xuid\":"+xv+"},";
		rl += pl;
	}
	mysql_free_result(res);
	mysql_close(con);
	return rl;
}

bool queryMysqlPlayerRemove(string name) {
	initDB();
	char queryC[400];
	sprintf(queryC, "DELETE FROM `allowlist` WHERE NAME='%s'",name.c_str());
	auto rt = mysql_real_query(con, queryC, strlen(queryC));
	res = mysql_store_result(con);
	mysql_free_result(res);
	mysql_close(con);
	if (!rt) {
		return true;
	}
	else {
		return false;
	}
	
}

bool queryMysqlPlayerAdd(string name, string xuid = "") {
	initDB();
	char queryC[400];
	sprintf(queryC, "INSERT INTO `allowlist`(`XUID`, `NAME`) VALUES ('%s','%s')", xuid.c_str(), name.c_str());
	auto rt = mysql_real_query(con, queryC, strlen(queryC));
	res = mysql_store_result(con);
	mysql_free_result(res);
	mysql_close(con);
	if (!rt) {
		return true;
	}
	else {
		return false;
	}
}

bool playerjoin(Event::PlayerPreJoinEvent ev) {
	bool pli = queryMysqlPlayerIn(ev.mPlayer->getRealName(),ev.mPlayer->getXuid());
	if (!pli) {
		ev.mPlayer->kick("\n您未注册账号，请前往官网注册后再进入游戏");
	}
	return true;
}

//注册指令
class AllowListCommand : public Command {
	enum AllowOP :int {
		add = 1,
		remove = 2,
		list = 3,
	} op;
	string dst;
public:
	void execute(CommandOrigin const& ori, CommandOutput& output) const override {//执行部分
		string dsxuid;
		//Player* p = Level::getPlayer(dst);
		dsxuid = PlayerInfo::getXuid(dst);
		switch (op) {
			case add:
				if (queryMysqlPlayerAdd(dst, dsxuid)) {
					output.addMessage("Add " + dst + " to allowlist success.");
				}
				else {
					output.addMessage("Add " + dst + " to allowlist failed.");
				}
				
				break;
			case remove:
				if (queryMysqlPlayerRemove(dst)) {
					output.addMessage("Remove " + dst + " from allowlist success.");
				}
				else {
					output.addMessage("Remove " + dst + " from allowlist failed.");
				}
				break;
			case list:
				string pll = "{\"command\":\"allowlist\",\"result\":[{"+queryMysqlPlayerList() + "}]}";
				output.addMessage(pll);
				break;
			}
	}

	static void setup(CommandRegistry* registry) {//注册部分(推荐做法)
		registry->registerCommand("mallowlist", "mysql allowlist", CommandPermissionLevel::GameMasters, { (CommandFlagValue)0 }, { (CommandFlagValue)0x80 });
		registry->addEnum<AllowOP>("allow1", { { "add",AllowOP::add},{ "remove",AllowOP::remove} });
		registry->addEnum<AllowOP>("allow2", { { "list", AllowOP::list} });

		registry->registerOverload<AllowListCommand>(
			"mallowlist",
			RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&AllowListCommand::op, "optional", "allow1"),
			RegisterCommandHelper::makeMandatory(&AllowListCommand::dst, "PlayerName")
			);
		registry->registerOverload<AllowListCommand>(
			"mallowlist",
			RegisterCommandHelper::makeMandatory<CommandParameterDataType::ENUM>(&AllowListCommand::op, "optional", "allow2"));
	}
};

void PluginInit()
{
	logger.info("AllowList Mysql Loaded.");
	Event::PlayerPreJoinEvent::subscribe(playerjoin);
	Event::ServerStartedEvent::subscribe([](Event::ServerStartedEvent ev) {
		bool db = initDB();
		if (db) {
			logger.info("Connected to database success.");
		}
		return true;
		});
	Event::RegCmdEvent::subscribe([](Event::RegCmdEvent ev) {
		AllowListCommand::setup(ev.mCommandRegistry);
		return true;
		});
}
