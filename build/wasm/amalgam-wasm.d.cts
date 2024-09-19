export = AmalgamRuntime;
declare function AmalgamRuntime<T extends EmscriptenModule = EmscriptenModule>(overrides?: Partial<T>): Promise<T>;
