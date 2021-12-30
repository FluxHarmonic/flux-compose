(define-module (flux graphics)
  #:use-module (system foreign)
  #:use-module (system foreign-library)
  #:export (show-preview-window render-graphic))

(define init-graphics
  ;; TODO: Don't use a brittle path to load the library
  (foreign-library-function (canonicalize-path "./bin/libflux.so") "init_graphics"
                            #:return-type void
                            #:arg-types (list int int)))

(define (show-preview-window width height)
  ;; Init SDL, renderer thread
  ;; Show the window
  (init-graphics width height)
  )

(define (render-graphic graphic)
  ;; Transmit instructions to renderer thread
  (let ((scene (assoc-ref graphic 'scene)))
    ))

(define (make-scene scene))

(define (make-filled-circle spec)
  (let ((x 500)
        (y 500)
        (radius 400)
        (r 255) (g 0) (b 0) (a 255)))
  (make-c-struct (list int16 int16 int16 uint8 uint8 uint8 uint8)
                 (list x y radius r g b a)))
