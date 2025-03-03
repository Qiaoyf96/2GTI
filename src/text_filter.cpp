#include "pisa/text_filter.hpp"

#include "gumbo.h"

namespace pisa {

TextFilter::TextFilter() = default;
TextFilter::TextFilter(TextFilter const&) = default;
TextFilter::TextFilter(TextFilter&&) = default;
TextFilter& TextFilter::operator=(TextFilter const&) = default;
TextFilter& TextFilter::operator=(TextFilter&&) = default;
TextFilter::~TextFilter() = default;

[[nodiscard]] auto cleantext(GumboNode* node) -> std::string
{
    if (node->type == GUMBO_NODE_TEXT) {
        return std::string(node->v.text.text);
    }
    if (node->type == GUMBO_NODE_ELEMENT && node->v.element.tag != GUMBO_TAG_SCRIPT
        && node->v.element.tag != GUMBO_TAG_STYLE) {
        std::string contents;
        GumboVector* children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            const std::string text = cleantext(reinterpret_cast<GumboNode*>(children->data[i]));
            if (i != 0 && not contents.empty() && not text.empty()) {
                contents.append(" ");
            }
            contents.append(text);
        }
        return contents;
    }
    return std::string();
}

auto StripHtmlFilter::filter(std::string_view input) -> std::string
{
    GumboOptions options = kGumboDefaultOptions;
    options.max_errors = 1000;
    GumboOutput* output = gumbo_parse_with_options(&options, input.data(), input.size());
    if (output->errors.length >= options.max_errors) {
        gumbo_destroy_output(&kGumboDefaultOptions, output);
        return std::string();
    }
    std::string content = cleantext(output->root);
    gumbo_destroy_output(&kGumboDefaultOptions, output);
    return content;
}

}  // namespace pisa
