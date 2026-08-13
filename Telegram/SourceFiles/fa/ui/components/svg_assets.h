#pragma once
#include <string_view>
namespace fa {
namespace svg {
constexpr std::string_view default_airplane = R"SVG(<?xml version="1.0" encoding="UTF-8"?>
<svg width="36px" height="36px" viewBox="0 0 108 108" xmlns="http://www.w3.org/2000/svg">
	<g transform="translate(0.000000,108.000000) scale(0.040000,-0.040000)" stroke="none" fill="none" fill-rule="evenodd">
		<path d="M1925 2059 c-245 -85 -1465 -618 -1509 -660 -66 -62 -37 -90 150 -148 272 -84 286 -81 711 201 438 290 448 296 448 231 0 -28 -112 -152 -276 -306 -208 -194 -275 -271 -269 -311 8 -57 530 -430 641 -458 118 -30 146 41 238 592 159 950 158 959 -134 859z" fill="#FFFFFF"></path>
	</g>
</svg>)SVG";
constexpr std::string_view default_all = R"SVG(<?xml version="1.0" encoding="UTF-8"?>
<svg width="36px" height="36px" viewBox="0 0 108 108" xmlns="http://www.w3.org/2000/svg">
	<g transform="translate(0.000000,108.000000) scale(0.040000,-0.040000)" stroke="none" fill="none" fill-rule="evenodd">
		<path d="M858 2130 c-151 -54 -332 -194 -407 -316 -133 -216 -122 -533 25 -737 95 -131 94 -210 -2 -307 -42 -42 -70 -83 -61 -91 31 -32 280 -10 375 33 70 31 166 46 325 48 260 3 391 51 561 204 164 148 213 262 213 499 0 172 -9 215 -67 315 -84 142 -219 263 -369 330 -161 71 -428 81 -593 22z" fill="#FFFFFF"></path>
		<path d="M2035 1687 c48 -115 47 -405 0 -541 -57 -161 -299 -421 -441 -472 -52 -19 -94 -42 -94 -51 0 -47 307 -63 416 -22 56 22 81 17 138 -25 65 -48 261 -74 241 -32 -5 10 -30 61 -56 112 -48 93 -48 94 4 150 147 162 183 422 86 624 -57 117 -117 182 -240 260 -73 47 -74 47 -54 -3z" fill="#FFFFFF"></path>
	</g>
</svg>)SVG";
constexpr std::string_view default_custom = R"SVG(<?xml version="1.0" encoding="UTF-8"?>
<svg width="36px" height="36px" viewBox="0 0 108 108" xmlns="http://www.w3.org/2000/svg">
	<g transform="translate(0.000000,108.000000) scale(0.040000,-0.040000)" stroke="none" fill="none" fill-rule="evenodd">
		<path d="M598 1952 c-73 -73 -73 -73 -73 -564 0 -686 -62 -638 825 -638 679 0 679 0 752 73 73 73 73 73 73 452 0 379 0 379 -73 452 -73 73 -75 73 -376 73 -332 0 -301 -12 -464 181 -31 37 -81 44 -314 44 -273 0 -278 -1 -350 -73z" fill="#FFFFFF"></path>
	</g>
</svg>)SVG";
constexpr std::string_view material_chat = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#e3e3e3"><path d="M240-400h320v-80H240v80Zm0-120h480v-80H240v80Zm0-120h480v-80H240v80ZM80-80v-720q0-33 23.5-56.5T160-880h640q33 0 56.5 23.5T880-800v480q0 33-23.5 56.5T800-240H240L80-80Zm126-240h594v-480H160v525l46-45Zm-46 0v-480 480Z"/></svg>)SVG";
constexpr std::string_view material_dock = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#e3e3e3"><path d="M200-120q-33 0-56.5-23.5T120-200v-560q0-33 23.5-56.5T200-840h560q33 0 56.5 23.5T840-760v560q0 33-23.5 56.5T760-120H200Zm440-80h120v-560H640v560Zm-80 0v-560H200v560h360Zm80 0h120-120Z"/></svg>)SVG";
constexpr std::string_view material_send = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#e3e3e3"><path d="M120-160v-640l760 320-760 320Zm80-120 474-200-474-200v140l240 60-240 60v140Zm0 0v-400 400Z"/></svg>)SVG";
} // namespace svg
} // namespace fa
