var OcctViewerModule =
{
    // print: (function() {
    //     var anElement = document.getElementById('output');
    //     return function(theText) { anElement.innerHTML += theText + "<br>"; };
    // })(),
    // printErr: function(theText) {
    //     //var anElement = document.getElementById('output');
    //     //anElement.innerHTML += theText + "<br>";
    //     console.warn(theText);
    // },
    canvas: (function() {
        var aCanvas = document.getElementById('occtViewerCanvas');
        var aGlCtx =                   aCanvas.getContext ('webgl2', { alpha: false, depth: true, antialias: false, preserveDrawingBuffer: true } );
        if (aGlCtx == null) { aGlCtx = aCanvas.getContext ('webgl',  { alpha: false, depth: true, antialias: false, preserveDrawingBuffer: true } ); }
        return aCanvas;
    })(),

    onRuntimeInitialized: function() {
        //console.log(" @@ onRuntimeInitialized()" + Object.getOwnPropertyNames(OcctViewerModule));
    }
};

const OcctViewerModuleInitialized = createOcctViewerModule(OcctViewerModule);
OcctViewerModuleInitialized.then(function(Module) {
    //OcctViewerModule.setCubemapBackground ("cubemap.jpg");
    //OcctViewerModule.openFromUrl ("ball", "samples/Ball.brep");
});