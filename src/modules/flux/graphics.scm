(define-module (flux graphics)
  #:use-module (system foreign)
  #:use-module (system foreign-library)
  #:use-module (ice-9 pretty-print)
  #:use-module (ice-9 iconv)
  #:use-module (ice-9 i18n)
  #:use-module (srfi srfi-9)
  #:export (show-preview-window
            render-graphic
            render-to-file
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

(define request-render-to-file
  (foreign-library-function (canonicalize-path "./build/libflux.so") "request_render_to_file"
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

(define (color->struct colorish)
  (if (string? colorish)
      (begin
        (string->color-struct colorish))
      (make-color-struct (color-r colorish)
                         (color-g colorish)
                         (color-b colorish)
                         (color-a colorish))))

(define (process-scene-member member)
  (let ((pointer
         (cond
          ((circle? member) (make-circle-struct (circle-pos-x member)
                                                (circle-pos-y member)
                                                (circle-radius member)
                                                (color->struct (circle-color member))))

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

(define (string->color-struct hex-string)
  (let ((r (locale-string->integer (substring hex-string 1 3) 16))
        (g (locale-string->integer (substring hex-string 3 5) 16))
        (b (locale-string->integer (substring hex-string 5 7) 16)))
    (make-color-struct r g b 255)))

(define (render-to-file file-name)
  ;; (bytevector->pointer (string->bytevector file-name "US-ASCII"))
  (pretty-print (string->bytevector file-name "US-ASCII"))
  (request-render-to-file))
