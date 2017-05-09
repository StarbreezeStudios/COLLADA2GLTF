#pragma once

#include <set>

#include "GeneratedSaxParser.h"
#include "COLLADASaxFWLLoader.h"
#include "COLLADASaxFWLIExtraDataCallbackHandler.h"

namespace COLLADA2GLTF {
	class ExtrasHandler : COLLADASaxFWL::IExtraDataCallbackHandler {
	private:
		virtual bool elementBegin(const COLLADASaxFWL::ParserChar* elementName, const GeneratedSaxParser::xmlChar** attributes);
		virtual bool elementEnd(const COLLADASaxFWL::ParserChar* elementName);
		virtual bool textData(const COLLADASaxFWL::ParserChar* text, size_t textLength) { return true; }

		virtual bool parseElement(
			const COLLADASaxFWL::ParserChar* profileName,
			const COLLADASaxFWL::StringHash& elementHash,
			const COLLADAFW::UniqueId& uniqueId,
			COLLADAFW::Object* object);

		COLLADASaxFWL::Loader* _loader;
		COLLADAFW::UniqueId _currentId;
		bool _inBump = false;
	public:
		std::set<COLLADAFW::UniqueId> lockAmbientDiffuse;
		COLLADAFW::TextureAttributes* bumpTexture = NULL;

		ExtrasHandler(COLLADASaxFWL::Loader* loader) : _loader(loader) {};
	};
}