#include "COLLADA2GLTFWriter.h"
#include "COLLADA2GLTFExtrasHandler.h"
#include "COLLADASaxFWLLoader.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "argparse.h"

#include <ctime>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <experimental/filesystem>
#include "collada2gltf.h"

using namespace std::experimental::filesystem;

extern "C" { int collada2gltf(int argc, char** argv) {
	GLTF::Asset* asset = new GLTF::Asset();
	COLLADA2GLTF::Options* options = new COLLADA2GLTF::Options();

	// Resolve and sanitize paths
	path inputPath;
	path basePath;

	inputPath = path(argv[1]);

	options->inputPath = inputPath.string();
	options->name = inputPath.stem().string();

	path outputPath;
	outputPath = inputPath.parent_path() / "output" / inputPath.stem();
	outputPath += ".gltf";

	options->outputPath = outputPath.string();

	// Create the output directory if it does not exist
	path outputDirectory = outputPath.parent_path();
	if (!std::experimental::filesystem::exists(outputDirectory)) {
		std::experimental::filesystem::create_directories(outputDirectory);
	}

	std::cout << "Converting " << options->inputPath << " -> " << options->outputPath << std::endl;
	std::clock_t start = std::clock();

	COLLADASaxFWL::Loader* loader = new COLLADASaxFWL::Loader();
	COLLADA2GLTF::ExtrasHandler* extrasHandler = new COLLADA2GLTF::ExtrasHandler(loader);
	COLLADA2GLTF::Writer* writer = new COLLADA2GLTF::Writer(asset, options, extrasHandler);
	loader->registerExtraDataCallbackHandler((COLLADASaxFWL::IExtraDataCallbackHandler*)extrasHandler);
	COLLADAFW::Root root(loader, writer);
	if (!root.loadDocument(options->inputPath)) {
		std::cout << "ERROR: Unable to load input from path '" << options->inputPath << "'" << std::endl;
		return -1;
	}

	asset->removeUnusedNodes();
	asset->removeUnusedSemantics();
	GLTF::Buffer* buffer = asset->packAccessors();

	// Create image bufferViews for binary glTF
	if (options->binary && options->embeddedTextures) {
		size_t imageBufferLength = 0;
		std::set<GLTF::Image*> images = asset->getAllImages();
		for (GLTF::Image* image : images) {
			imageBufferLength += image->byteLength;
		}
		unsigned char* bufferData = buffer->data;
		bufferData = (unsigned char*)realloc(bufferData, buffer->byteLength + imageBufferLength);
		size_t byteOffset = buffer->byteLength;
		for (GLTF::Image* image : images) {
			GLTF::BufferView* bufferView = new GLTF::BufferView(byteOffset, image->byteLength, buffer);
			image->bufferView = bufferView;
			std::memcpy(bufferData + byteOffset, image->data, image->byteLength);
			byteOffset += image->byteLength;
		}
		buffer->data = bufferData;
		buffer->byteLength += imageBufferLength;
		asset->extensions.insert("KHR_binary_glTF");
	}

	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> jsonWriter = rapidjson::Writer<rapidjson::StringBuffer>(s);
	jsonWriter.StartObject();
	asset->writeJSON(&jsonWriter, options);
	jsonWriter.EndObject();

	if (!options->embeddedTextures) {
		for (GLTF::Image* image : asset->getAllImages()) {
			path uri = outputDirectory / image->uri;
			FILE* file = fopen(uri.generic_string().c_str(), "wb");
			if (file != NULL) {
				fwrite(image->data, sizeof(unsigned char), image->byteLength, file);
				fclose(file);
			}
			else {
				std::cout << "ERROR: Couldn't write image to path '" << uri << "'" << std::endl;
			}
		}
	}

	if (!options->embeddedBuffers) {
		path uri = outputDirectory / buffer->uri;
		FILE* file = fopen(uri.generic_string().c_str(), "wb");
		if (file != NULL) {
			fwrite(buffer->data, sizeof(unsigned char), buffer->byteLength, file);
			fclose(file);
		}
		else {
			std::cout << "ERROR: Couldn't write buffer to path '" << uri << "'" << std::endl;
		}
	}

	if (!options->embeddedShaders) {
		for (GLTF::Shader* shader : asset->getAllShaders()) {
			path uri = outputDirectory / shader->uri;
			FILE* file = fopen(uri.generic_string().c_str(), "wb");
			if (file != NULL) {
				fwrite(shader->source.c_str(), sizeof(unsigned char), shader->source.length(), file);
				fclose(file);
			}
			else {
				std::cout << "ERROR: Couldn't write shader to path '" << uri << "'" << std::endl;
			}
		}
	}

	std::string jsonString = s.GetString();
	if (!options->binary) {
		rapidjson::Document jsonDocument;
		jsonDocument.Parse(jsonString.c_str());

		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		jsonDocument.Accept(writer);

		std::ofstream file(options->outputPath);
		if (file.is_open()) {
			file << buffer.GetString() << std::endl;
			file.close();
		}
		else {
			std::cout << "ERROR: couldn't write glTF to path '" << options->outputPath << "'" << std::endl;
		}
	}
	else {
		if (outputPath.extension() != "glb") {
			outputPath = outputPath.parent_path() / outputPath.stem();
			outputPath += ".glb";
		}
		FILE* file = fopen(outputPath.generic_string().c_str(), "wb");
		if (file != NULL) {
			uint32_t* writeHeader = new uint32_t[2];
			fwrite("glTF", sizeof(char), 4, file); // magic
			writeHeader[0] = 2; // version
			int padding = (20 + jsonString.length()) % 4;
			if (padding != 0) {
				padding = 4 - padding;
			}
			writeHeader[1] = 20 + jsonString.length() + padding + buffer->byteLength; // length
			fwrite(writeHeader, sizeof(uint32_t), 2, file);
			writeHeader[0] = jsonString.length() + padding; // chunkLength
			writeHeader[1] = 0x4E4F534A; // chunkType JSON
			fwrite(writeHeader, sizeof(uint32_t), 2, file);
			fwrite(jsonString.c_str(), sizeof(char), jsonString.length(), file);
			for (int i = 0; i < padding; i++) {
				fwrite(" ", sizeof(char), 1, file);
			}
			writeHeader[0] = buffer->byteLength; // chunkLength
			writeHeader[1] = 0x004E4942; // chunkType BIN
			fwrite(writeHeader, sizeof(uint32_t), 2, file);
			fwrite(buffer->data, sizeof(unsigned char), buffer->byteLength, file);
			fclose(file);
		}
		else {
			std::cout << "ERROR couldn't write binary glTF to path '" << outputPath << "'" << std::endl;
		}
	}

	std::clock_t end = std::clock();
	std::cout << "Time: " << ((end - start) / (double)(CLOCKS_PER_SEC / 1000)) << " ms" << std::endl;

	return 0;
}
}
