#include "Atlas.h"
#include "tinyxml2.h"
#include <cstdio>
#include <cstdlib>

bool LoadAtlas(const std::string& xmlPath, Atlas& out) {
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(xmlPath.c_str()) != tinyxml2::XML_SUCCESS) {
        std::fprintf(stderr, "Atlas: cannot parse %s: %s\n", xmlPath.c_str(), doc.ErrorStr());
        return false;
    }

    auto* root = doc.FirstChildElement("Atlas");
    if (!root) return false;

    if (auto* tex = root->FirstChildElement("Texture")) {
        const char* fn = tex->Attribute("filename");
        if (fn) out.textureFile = fn;
    }

    auto* elems = root->FirstChildElement("Elements");
    if (!elems) return !out.textureFile.empty();

    for (auto* el = elems->FirstChildElement("Element"); el; el = el->NextSiblingElement("Element")) {
        const char* name = el->Attribute("name");
        if (!name) continue;
        AtlasElement e{};
        el->QueryFloatAttribute("u1", &e.u1);
        el->QueryFloatAttribute("u2", &e.u2);
        el->QueryFloatAttribute("v1", &e.v1);
        el->QueryFloatAttribute("v2", &e.v2);
        out.elements[name] = e;
    }

    return !out.textureFile.empty();
}
