# Drawy / Okular freehand annotation experiment

This experiment uses Approach B: extract the useful Drawy-style freehand behavior into the existing Okular ink annotation pipeline instead of grafting Drawy's full application model.

Why this boundary is smaller and safer:

- Okular already owns the PDF page, page annotation model, undo/redo, rendering, and Poppler persistence.
- Drawy contributes a useful stroke pipeline: pressure-bearing input, point smoothing, and live rendering treatment.
- The final persistent document representation must remain an ordinary Okular::InkAnnotation so the existing save/reopen lifecycle stays intact.
- The full Drawy app stack (canvas, command history, scene management, JSON persistence, etc.) is not necessary for an experimental integration.

Data flow after the experiment:

QTabletEvent / QMouseEvent
  -> PageView::tabletEvent() / PageViewAnnotator::routeMouseEvent()
  -> AnnotatorEngine::decodeEvent() with pressure preserved
  -> SmoothPathEngine::event()
  -> Drawy-inspired moving-average filtering + pressure tracking
  -> Okular::InkAnnotation with filtered normalized points
  -> Document::addPageAnnotation()
  -> existing Okular undo/redo and PDF save workflow

Tablet pressure is normalized with the same `pressure / 1.60 + 0.375` response used by Drawy. Mouse input uses pressure 1.0. The processor applies Drawy's causal seven-sample moving average as each point arrives, so already-painted points do not move when later samples arrive. Okular's existing annotator paint layer acts as the live preview overlay: each processed segment invalidates its old/new area, including its pressure-scaled pen radius, and is repainted with round caps/joins and antialiasing. The completed annotation stores only the processed normalized points and a representative uniform width because standard PDF Ink has no per-point pressure field.

The session-only extension stores the processed pressure list and representative width on the one `InkAnnotation` object. `PagePainter` uses that data after release for pressure-aware rendering; `InkAnnotation::store()` does not serialize it. Undo and redo therefore retain the pressure data while the annotation object remains in the session, while saving and reopening falls back to the ordinary uniform PDF Ink representation.

This leaves Okular's mature annotation model and Poppler pipeline intact while testing whether the Drawy freehand behavior can be reused at the geometry-processing seam.
