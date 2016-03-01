#!/bin/bash

#throw all the text into one big header
code=
for f in "${@}"; do
	code="$(xxd -i ${f})
${code}"
done

#have a map to iterate over
map="$(echo "${code}" | grep -F unsigned | sed -e "s/.* \([a-Z_]\+\)\[\] =.*/{\"\1\",{\1,\1+/g" -e "s/.* \([a-Z_]\+\) =.*/\1}},/g" | tr -d '\n' | sed -e "s/}},{/}},\n  {/g")"
for key in $(echo "${map}" | grep -F '_json"' | sed -e "s/.*{\"//g" -e "s/\",.*//g"); do
	k="$(echo "${key}" | sed -e "s/_json//g" -e "s/_/-/g")"
	map="$(echo "${map}" | sed -e "s/\"${key}\"/\"${k}\"/g")"
done

echo "#include <unordered_map>"
echo "${code}"
echo "const std::unordered_map<std::string, std::string> locales_json = {";
echo "  ${map}"
echo "};";
