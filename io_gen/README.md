# Input-Output generation

## Value profile
If you want to automatically generate IO examples to a C function, you first need to specify a value profile file. A value profile is a JSON file containing specification of array dimensions and values of integer variables. The value profile must contain an entry for each argument of the C function. For example, consider the function below:

```c
void scale_array(int *a, int s, int n){
  int i;
  for(i = 0; i < n; ++i){
    a[i] *= s;
  }
}
```

a possible value profile is shown below:

```yaml
{
  a: 10000,
  s: 3,
  n: 10000
}
```
In the profile above, array `a` is specified as having 10000 elements. Variable `n` has the value 10000 since is used as control variable for the loop that traverses `a`. Variable `s` will have the value 3 in the generated IO examples

## IO generation
```
python3 io_gen.py <path-to-c-program> -vp <path-to-value-profile-file> -ov output_variable_name -n numer_of_inputs
```
## Restrictions
