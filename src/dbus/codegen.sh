echo you should never need to run this, unless you change sniffer.xml as the generated code will be committed with any interface definition updates
gdbus-codegen --generate-c-code sniffer-generated --c-namespace pi --interface-prefix com.signswift sniffer.xml
echo "code generation complete"

