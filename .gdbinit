handle SIGUSR1 nostop noprint pass

define pt
  p (flanchor *)(($arg0).constexp & ~15)
end

define fl
  set $a = (flanchor *)(($arg0).constexp & ~15)
  set $n = (flanchor *)(($a->n).constexp & ~15)
  set $p = (flanchor *)(($a->p).constexp & ~15)
  printf "a: %p n: %p p: %p\n", $a, $n, $p
end

define cof
  p ($arg1 *)((uptr) $arg0 - (uptr) &(($arg1 *) 0)->$arg2)
end

define cof_aligned
  p ($arg1 *) ((uptr) $arg0 - (uptr) $arg0 % sizeof($arg1))
end
