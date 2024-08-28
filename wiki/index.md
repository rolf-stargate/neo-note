# NeoNote || project:neo_note
## Notes
### Insert New Line
```sh
[
> [___abc]
  [___abc]
  [___abc]
  _
  _
  _
]
```

```sh
[
  _
  _
  _
> [___abc]
  [___abc]
  [___abc]
]
```

```sh
[
  _
  _
  _
  [___abc]
  [___abc]
> [___abc]
]
```

### Handle Keys
#### Enter **DONE**
```
[___abcP]
       ^
```
- insert_new_line
- cursor->line + 1

```
[ab___cP]
 ^
```
- insert_new_line

```
[___abcP]a
     ^
1. [___P]b     

2. [a___bcP]a

3. [___P]b     
       ^ 
4. [bc_P]b
       ^
5. [a_____P]a
       
```
1. insert_new_line
2. move line_a->gap_end cursor->pos -1
3. cursor->line++ and cursor->pos = line_b->buf_size - 1
4. copy cursor->pos - line_a->buf_size - 2 to new line
5. line_a->gap_end + (line_a->buf_size - 2 - line_a->gap_end)

```
[ab___cP]a
  ^
```
- same as before

#### Backspace  **DONE**
1.
```
[___abcP]
      ^
[ab___cP]
      ^
[a____cP]
      ^
```
- move gap_end to pos - 1 **DONE** 
- gap_start--  **DONE**

2.
```
[abc___P]
   ^
[ab___cP]
   ^
[a____cP]
   ^
[a____cP]
      ^
```
- move gap_start to pos **DONE**
- gap_start-- **DONE**
- pos = gap_end + 1 **DONE**

3.
```
[abc___P]
 ^
```
- if previous line switch to previous line **DONE**

4.
```
[___abcP]
    ^
```
- if previous line switch to previous line **DONE**

5.
```
[abc___P] 
       ^
[ab____P] 
       ^
```
- gap_start-- **DONE**

6.
```
[a___bcP]
     ^
[____bcP]
     ^
```
- gap_start-- **DONE**

7.
```
[a___bcP]
      ^
[a____cP]
      ^
```
- gap_end++ **DONE**


## TODAY | due:today or due:tomorrow or status:overdue 

## TODO | status:pending -bug due.not:tomorrow due.not:today
* [ ] check that there is no buffer overflow anywhere  #f4596c3d


## Bugs | status:pending +bug due.not:tomorrow due.not:today

## Archive | status:Completed
* [X] implement backspace  #7dc60527
* [X] implement enter  #33492660
