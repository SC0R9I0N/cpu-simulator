#include "XLSXEventLogger.h"

#include <stdexcept>

XLSXEventLogger::XLSXEventLogger(const std::string& outputPath)
	: worksheetName("Sheet1"), nextRow(2), currentCycle(0), open(false)
{
	document.create(outputPath, true);
	auto worksheet = document.workbook().worksheet(worksheetName);
	worksheet.cell("A1").value() = "Seq";
	worksheet.cell("B1").value() = "PC";
	worksheet.cell("C1").value() = "Instruction";
	open = true;
}

XLSXEventLogger::~XLSXEventLogger()
{
	close();
}

void XLSXEventLogger::startCycle()
{
	if (!open)
		return;

	currentCycle++;

	// Cycles are contiguous and 1-based, so column is deterministic:
	// cycle 1 -> col 4 (D), cycle 2 -> col 5 (E), ...
	int col = currentCycle + 3;
	auto worksheet = document.workbook().worksheet(worksheetName);
	worksheet.cell(OpenXLSX::XLCellReference(1, col)).value() = currentCycle;
}

int XLSXEventLogger::ensureInstructionRow(int sequence, int pc, const std::string& instruction)
{
	auto existing = instructionRow.find(sequence);
	if (existing != instructionRow.end())
		return existing->second;

	int row = nextRow++;
	instructionRow[sequence] = row;
	auto worksheet = document.workbook().worksheet(worksheetName);
	worksheet.cell(OpenXLSX::XLCellReference(row, 1)).value() = sequence;
	worksheet.cell(OpenXLSX::XLCellReference(row, 2)).value() = pc;
	worksheet.cell(OpenXLSX::XLCellReference(row, 3)).value() = instruction;
	return row;
}

std::string XLSXEventLogger::readCellAsString(int row, int col)
{
	auto worksheet = document.workbook().worksheet(worksheetName);

	try
	{
		return worksheet.cell(OpenXLSX::XLCellReference(row, col)).value().get<std::string>();
	}
	catch (const std::exception&)
	{
		return "";
	}
}

void XLSXEventLogger::logFetchedInstruction(int sequence, int pc, const std::string& instruction)
{
	if (!open)
		return;

	if (currentCycle <= 0)
		return;

	ensureInstructionRow(sequence, pc, instruction);
}

void XLSXEventLogger::markStage(int sequence, const std::string& stage)
{
	if (!open)
		return;

	if (currentCycle <= 0)
		return;

	if (instructionRow.count(sequence) == 0)
		return;

	int row = instructionRow[sequence];
	int col = currentCycle + 3;
	auto worksheet = document.workbook().worksheet(worksheetName);
	worksheet.cell(OpenXLSX::XLCellReference(row, col)).value() = stage;
}

void XLSXEventLogger::close()
{
	if (!open)
		return;

	auto worksheet = document.workbook().worksheet(worksheetName);
	int legendRow = nextRow + 1;
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 1)).value() = "[Legend]";

	legendRow++;
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 1)).value() = "IF";
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 2)).value() = "Instruction Fetch";

	legendRow++;
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 1)).value() = "ID";
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 2)).value() = "Instruction Decode";

	legendRow++;
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 1)).value() = "IS";
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 2)).value() = "Add to Issue Queue";

	legendRow++;
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 1)).value() = "EX";
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 2)).value() = "Execute (instruction can execute on the same cycle as it is added to the issue queue, if it is ready)";

	legendRow++;
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 1)).value() = "WB";
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 2)).value() = "Write Back";

	legendRow++;
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 1)).value() = "CT";
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 2)).value() = "Commit (instruction can commit on the same cycle as write back, if at the head)";

	legendRow++;
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 1)).value() = "RS Stall";
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 2)).value() = "Reservation Station Stall (stalled because reservation stations are full)";

	legendRow++;
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 1)).value() = "ROB Stall";
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 2)).value() = "Reorder Buffer Stall (stalled because the ROB is full)";

	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 1)).value() = "CDB Stall";
	worksheet.cell(OpenXLSX::XLCellReference(legendRow, 2)).value() = "Common Data Bus Stall (stalled because the CDB bandwidth is saturated)";

	// Fix timestamps to a constant so that files with identical content produce identical bytes
	document.setProperty(OpenXLSX::XLProperty::CreationDate,     "2000-01-01T00:00:00Z");
	document.setProperty(OpenXLSX::XLProperty::ModificationDate, "2000-01-01T00:00:00Z");

	document.save();
	document.close();
	open = false;
}