use std::num::Wrapping;

pub fn test(a1: i32, a2: i32) -> i32
{
    (Wrapping(a1) + Wrapping(a2)).0
}

pub struct Foo
{
    Field: i32
}

impl Foo
{
    pub fn new(field: i32) -> Foo
    {
        Foo{ Field: field }
    }
}

pub struct Bar
{
}

impl Bar
{
    pub fn new() -> Bar
    {
        Bar{}
    }
}

pub trait GetValueTrait
{
    fn GetValue(&self) -> i32;

    fn ToDyn(&self) -> &dyn GetValueTrait
        where Self: std::marker::Sized
    {
        self
    }
}

impl GetValueTrait for Foo
{
    fn GetValue(&self) -> i32
    {
        self.Field
    }
}

impl GetValueTrait for Bar
{
    fn GetValue(&self) -> i32
    {
        233
    }
}

pub fn GetValue(o : &dyn GetValueTrait) -> i32
{
    o.GetValue()
}

pub fn mian() -> i32
{
    let foo = Foo::new(123);
    let bar = Bar::new();
    test(GetValue((&foo).ToDyn()), GetValue((&bar).ToDyn()))
}
