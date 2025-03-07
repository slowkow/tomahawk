#ifndef IO_BCF_BCFREADER_CPP_
#define IO_BCF_BCFREADER_CPP_

#include "../vcf/VCFHeader.h"
#include "BCFReader.h"

namespace Tomahawk{
namespace BCF{

BCFReader::BCFReader() : filesize(0), current_pointer(0){}
BCFReader::~BCFReader(){}


bool BCFReader::nextBlock(void){
	// Stream died
	if(!this->stream.good()){
		std::cerr << Tomahawk::Helpers::timestamp("ERROR", "BCF") << "Stream died!" << std::endl;
		return false;
	}

	// EOF
	if(this->stream.tellg() == this->filesize)
		return false;

	if(!this->bgzf_controller.InflateBlock(this->stream, this->buffer)){
		return false;
	}

	// Reset buffer
	this->buffer.reset();
	this->current_pointer = 0;

	return true;
}

bool BCFReader::nextVariant(BCFEntry& entry){
	if(this->current_pointer == this->bgzf_controller.buffer.size()){
		if(!this->nextBlock())
			return false;
	}

	if(this->current_pointer + 8 > this->bgzf_controller.buffer.size()){
		const S32 partial = (S32)this->bgzf_controller.buffer.size() - this->current_pointer;
		entry.add(&this->bgzf_controller.buffer[this->current_pointer], this->bgzf_controller.buffer.size() - this->current_pointer);
		if(!this->nextBlock()){
			std::cerr << Tomahawk::Helpers::timestamp("ERROR","BCF") << "Failed to get next block in partial" << std::endl;
			return false;
		}

		entry.add(&this->bgzf_controller.buffer[0], 8 - partial);
		this->current_pointer = 8 - partial;
	} else {
		entry.add(&this->bgzf_controller.buffer[this->current_pointer], 8);
		this->current_pointer += 8;
	}

	U64 remainder = entry.sizeBody();
	while(remainder > 0){
		if(this->current_pointer + remainder > this->bgzf_controller.buffer.size()){
			entry.add(&this->bgzf_controller.buffer[this->current_pointer], this->bgzf_controller.buffer.size() - this->current_pointer);
			remainder -= this->bgzf_controller.buffer.size() - this->current_pointer;
			if(!this->nextBlock())
				return false;

		} else {
			entry.add(&this->bgzf_controller.buffer[this->current_pointer], remainder);
			this->current_pointer += remainder;
			remainder = 0;
			break;
		}
	}
	entry.parse();

	return true;
}

bool BCFReader::parseHeader(void){
	if(this->bgzf_controller.buffer.size() == 0){
		std::cerr << Tomahawk::Helpers::timestamp("ERROR","BCF") << "No buffer!" << std::endl;
		return false;
	}

	if(this->bgzf_controller.buffer.size() < 5){
		std::cerr << Tomahawk::Helpers::timestamp("ERROR","BCF") << "Corrupted header!" << std::endl;
		return false;
	}

	if(strncmp(&this->bgzf_controller.buffer.data[0], "BCF\2\2", 5) != 0){
		std::cerr << Tomahawk::Helpers::timestamp("ERROR","BCF") << "Failed to validate MAGIC" << std::endl;
		return false;
	}

	const U32 l_text = *reinterpret_cast<const U32* const>(&this->bgzf_controller.buffer[5]) + 4;
	this->header_buffer.resize(l_text + 1);

	if(l_text - 5 < this->bgzf_controller.buffer.size()){
		this->header_buffer.Add(&this->bgzf_controller.buffer[5], l_text);
		this->current_pointer = l_text + 5;
	} else {
		U32 head_read = this->bgzf_controller.buffer.size() - 5;
		this->header_buffer.Add(&this->bgzf_controller.buffer[5], this->bgzf_controller.buffer.size() - 5);

		//U32 p = 0;
		while(this->nextBlock()){
			if(head_read + this->bgzf_controller.buffer.size() >= l_text){
				this->header_buffer.Add(&this->bgzf_controller.buffer[0], l_text - head_read);
				this->current_pointer = l_text - head_read;
				break;
			}
			head_read += this->bgzf_controller.buffer.size();
			this->header_buffer.Add(&this->bgzf_controller.buffer[0], this->bgzf_controller.buffer.size());
		}
	}

	if(!this->header.parse(&this->header_buffer[0], this->header_buffer.size())){
		std::cerr << Tomahawk::Helpers::timestamp("ERROR","BCF") << "Failed to parse header!" << std::endl;
		return false;
	}

	return true;
}

bool BCFReader::open(const std::string input){
	this->stream.open(input, std::ios::binary | std::ios::in | std::ios::ate);
	if(!this->stream.good()){
		std::cerr << Tomahawk::Helpers::timestamp("ERROR", "BCF") << "Failed to open file: " << input << std::endl;
		return false;
	}

	this->filesize = this->stream.tellg();
	this->stream.seekg(0);

	if(!this->stream.good()){
		std::cerr << Tomahawk::Helpers::timestamp("ERROR", "BCF") << "Bad stream!" << std::endl;
		return false;
	}

	if(!this->nextBlock()){
		std::cerr << Tomahawk::Helpers::timestamp("ERROR","BCF") << "Failed to get first block!" << std::endl;
		return false;
	}

	if(!this->parseHeader()){
		std::cerr << Tomahawk::Helpers::timestamp("ERROR","BCF") << "Failed to parse header!" << std::endl;
		return false;
	}

	return true;
}

}
}

#endif /* IO_BCF_BCFREADER_CPP_ */
