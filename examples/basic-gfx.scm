
;; Display context: is it in the REPL or run as a shell script, i.e. should we show a window?

;; GOAL: Draw a red square centered on a white background

(use-modules (flux graphics))

(show-preview-window 1280 720)

(define stream-bg '((width  . 1000)
                    (height . 1000)
                    (scene . ((filled-circle #:pos (500 500)
                                             #:radius 400
                                             #:color (255 0 0 255))))))

(render-graphic stream-bg)
