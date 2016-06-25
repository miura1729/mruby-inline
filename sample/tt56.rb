include Inline
def foo
  1
end
make_inline_method  :foo
100000000.times do
  foo
end
