#ifndef RV32SIM_XLSX_EVENT_LOGGER_H
#define RV32SIM_XLSX_EVENT_LOGGER_H

#include <OpenXLSX.hpp>
#include <string>
#include <unordered_map>

class XLSXEventLogger
{
public:
	XLSXEventLogger(const std::string& outputPath);
	~XLSXEventLogger();

	void startCycle();
	void logFetchedInstruction(int sequence, int pc, const std::string& instruction);
	void markStage(int sequence, const std::string& stage);
	void close();

private:
	int ensureInstructionRow(int sequence, int pc, const std::string& instruction);
	int getStagePriority(const std::string& stage) const;
	std::string readCellAsString(int row, int col);

	OpenXLSX::XLDocument document;
	std::string worksheetName;
	std::unordered_map<int, int> instructionRow;
	int nextRow;
	int currentCycle;
	bool open;
};

#endif // RV32SIM_XLSX_EVENT_LOGGER_H