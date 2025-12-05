#pragma once

#include <string>

namespace http {

/**
 * Escape a string for safe inclusion in HTML content.
 * Converts special HTML characters to their entity equivalents:
 * - & → &amp;
 * - < → &lt;
 * - > → &gt;
 * - " → &quot;
 * - ' → &#39;
 *
 * @param s The string to escape
 * @return The HTML-escaped string
 */
std::string escapeHtml(const std::string& s);

}  // namespace http
