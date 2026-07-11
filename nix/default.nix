{
  callPackage,
  lib,
  stdenv,
  pname ? "fagram-desktop",
  tg_owt,
  unwrapped ? callPackage ./unwrapped.nix { inherit tg_owt; },
  qtbase,
  qtimageformats,
  qtsvg,
  qtwayland,
  kimageformats,
  wrapGAppsHook3,
  wrapQtAppsHook,
  glib-networking,
  webkitgtk_4_1,
  withWebkit ? true,
}:

stdenv.mkDerivation (finalAttrs: {
  inherit pname;
  inherit (finalAttrs.unwrapped) version meta passthru;
  inherit unwrapped;

  nativeBuildInputs = [ wrapQtAppsHook ]
    ++ lib.optionals withWebkit [ wrapGAppsHook3 ];

  buildInputs = [ qtbase qtimageformats qtsvg kimageformats qtwayland ]
    ++ lib.optionals withWebkit [ glib-networking ];

  qtWrapperArgs = lib.optionals withWebkit [
    "--prefix" "LD_LIBRARY_PATH" ":" (lib.makeLibraryPath [ webkitgtk_4_1 ])
  ];

  dontUnpack = true;

  installPhase = ''
    runHook preInstall
    cp -r "$unwrapped" "$out"
    runHook postInstall
  '';

  preFixup = lib.optionalString withWebkit ''
    qtWrapperArgs+=("''${gappsWrapperArgs[@]}")
  '';

  postFixup = ''
    substituteInPlace $out/share/dbus-1/services/* \
      --replace-fail "$unwrapped" "$out"
  '';
})
