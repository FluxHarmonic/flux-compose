(define-module (flux graphics)
  #:use-module (system foreign)
  #:use-module (system foreign-library)
  #:use-module (ice-9 pretty-print)
  #:use-module (srfi srfi-9)
  #:export (show-preview-window
            render-graphic
            make-circle
            make-color))

(define-record-type <color>
  (make-color r g b a)
  color?
  (r color-r)
  (g color-g)
  (b color-b)
  (a color-a))

(define-record-type <circle>
  (make-circle pos-x pos-y radius color)
  circle?
  (pos-x circle-pos-x)
  (pos-y circle-pos-y)
  (radius circle-radius)
  (color circle-color))

(define color-struct (list uint8 uint8 uint8 uint8))

(define make-color-struct
  (foreign-library-function (canonicalize-path "./build/libflux.so") "make_color_struct"
                            #:return-type '*
                            #:arg-types (list uint8 uint8 uint8 uint8)))

(define make-circle-struct
  (foreign-library-function (canonicalize-path "./build/libflux.so") "make_circle_struct"
                            #:return-type '*
                            #:arg-types (list int16 int16 int16 '*)))
(define add-scene-member
  (foreign-library-function (canonicalize-path "./build/libflux.so") "add_scene_member"
                            #:return-type void
                            #:arg-types (list uint32 '*)))

(define promote-staging-scene
  (foreign-library-function (canonicalize-path "./build/libflux.so") "promote_staging_scene"
                            #:return-type void))

(define init-graphics
  ;; TODO: Don't use a brittle path to load the library
  (foreign-library-function (canonicalize-path "./build/libflux.so") "init_graphics"
                            #:return-type void
                            #:arg-types (list int int)))

(define (show-preview-window width height)
  ;; Init SDL, renderer thread
  ;; Show the window
  (init-graphics width height))

(define (render-graphic graphic)
  ;; Transmit instructions to renderer thread
  (let ((scene (assoc-ref graphic 'scene)))
    (make-scene scene)
    (promote-staging-scene)))

(define (process-scene-member member)
  (let ((pointer
         (cond
          ((circle? member) (make-circle-struct (circle-pos-x member)
                                                (circle-pos-y member)
                                                (circle-radius member)
                                                (let ((color (circle-color member)))
                                                  (make-color-struct (color-r color)
                                                                     (color-g color)
                                                                     (color-b color)
                                                                     (color-a color)))))

          (else (format #t "Unknown member type: ~s" member)
                #f))))
    (when pointer
      (add-scene-member 2 pointer))))

(define (make-scene scene)
  ;; (pretty-print scene)
  (let ((scene-members (map process-scene-member scene)))
    (pretty-print scene-members))
  #t)

(define (plist-ref key lst)
  (let ((res (memq key lst)))
    (if res (cadr res) #f)))
