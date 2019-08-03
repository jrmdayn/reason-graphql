open Jest;
open Expect;
open Graphql.Language;
open GraphqlFuture;

let schema = StarWarsSchema.schema;

let okResponse = data => Schema.dataToConstValue((data, []));

let expectResponse = (~variables=[], query, expected, assertion) => {
  Schema.execute(schema, ~document=Parser.parseExn(query), ~variables, ~ctx=())
  ->Schema.Io.Result.map(
      fun
      | `Response(response) => {
          let actual = Graphql_Json.fromConstValue(response);
          let expected = Graphql_Json.fromConstValue(expected);
          assertion(expect(actual) |> toEqual(expected));
        }
      | `Stream(_) => failwith("Unexpected stream response"),
    )
  ->ignore;
};

describe("Basic Queries", () => {
  testAsync("Correctly identifies R2-D2 as the hero of the Star Wars Saga", assertion => {
    let query = {|
      query HeroNameQuery {
        hero {
          name
        }
      }
    |};

    let expected = okResponse(`Map([("hero", `Map([("name", `String("R2-D2"))]))]));

    expectResponse(query, expected, assertion);
  });

  testAsync("Allows us to query for the ID and friends of R2-D2", assertion => {
    let query = {|
        query HeroNameAndFriendsQuery {
          hero {
            id
            name
            friends {
              name
            }
          }
        }
    |};

    let expected =
      okResponse(
        `Map([
          (
            "hero",
            `Map([
              ("id", `Int(2001)),
              ("name", `String("R2-D2")),
              (
                "friends",
                `List([
                  `Map([("name", `String("Luke Skywalker"))]),
                  `Map([("name", `String("Han Solo"))]),
                  `Map([("name", `String("Leia Organa"))]),
                ]),
              ),
            ]),
          ),
        ]),
      );

    expectResponse(query, expected, assertion);
  });
});

describe("Nested Queries", () =>
  testAsync("Allows us to query for the friends of friends of R2-D2", assertion => {
    let query = {|
      query NestedQuery {
        hero {
          name
          friends {
            name
            appearsIn
            friends {
              name
            }
          }
        }
      }
    |};

    let expected =
      okResponse(
        `Map([
          (
            "hero",
            `Map([
              ("name", `String("R2-D2")),
              (
                "friends",
                `List([
                  `Map([
                    ("name", `String("Luke Skywalker")),
                    (
                      "appearsIn",
                      `List([`String("NEWHOPE"), `String("EMPIRE"), `String("JEDI")]),
                    ),
                    (
                      "friends",
                      `List([
                        `Map([("name", `String("Han Solo"))]),
                        `Map([("name", `String("Leia Organa"))]),
                        `Map([("name", `String("C-3PO"))]),
                        `Map([("name", `String("R2-D2"))]),
                      ]),
                    ),
                  ]),
                  `Map([
                    ("name", `String("Han Solo")),
                    (
                      "appearsIn",
                      `List([`String("NEWHOPE"), `String("EMPIRE"), `String("JEDI")]),
                    ),
                    (
                      "friends",
                      `List([
                        `Map([("name", `String("Luke Skywalker"))]),
                        `Map([("name", `String("Leia Organa"))]),
                        `Map([("name", `String("R2-D2"))]),
                      ]),
                    ),
                  ]),
                  `Map([
                    ("name", `String("Leia Organa")),
                    (
                      "appearsIn",
                      `List([`String("NEWHOPE"), `String("EMPIRE"), `String("JEDI")]),
                    ),
                    (
                      "friends",
                      `List([
                        `Map([("name", `String("Luke Skywalker"))]),
                        `Map([("name", `String("Han Solo"))]),
                        `Map([("name", `String("C-3PO"))]),
                        `Map([("name", `String("R2-D2"))]),
                      ]),
                    ),
                  ]),
                ]),
              ),
            ]),
          ),
        ]),
      );

    expectResponse(query, expected, assertion);
  })
);

describe("Mutation operation", () => {
  let mutation = {|
    mutation MyMutation($id: Int!, $name: String!){
      updateCharacterName(characterId: $id, name: $name) {
        character {
          id
          name
        }
        error
      }
    }
  |};

  let variables =
    "{\"id\": 1000, \"name\": \"Sikan Skywalker\"}"
    ->Js.Json.parseExn
    ->Graphql_Json.toVariables
    ->Belt.Result.getExn;

  testAsync("returns the right data", assertion => {
    let expected =
      okResponse(
        `Map([
          (
            "updateCharacterName",
            `Map([
              (
                "character",
                `Map([("id", `Int(1000)), ("name", `String("Sikan Skywalker"))]),
              ),
              ("error", `Null),
            ]),
          ),
        ]),
      );

    expectResponse(mutation, expected, assertion, ~variables);
  });
});

describe("Using aliases to change the key in the response", () => {
  testAsync("Allows us to query for Luke, changing his key with an alias", assertion => {
    let query = {|
      query FetchLukeAliased {
        luke: human(id: 1000) {
          name
        }
      }
    |};

    let expected = okResponse(`Map([("luke", `Map([("name", `String("Luke Skywalker"))]))]));

    expectResponse(query, expected, assertion);
  });

  testAsync(
    "Allows us to query for both Luke and Leia, using two root fields and an alias", assertion => {
    let query = {|
      query FetchLukeAndLeiaAliased {
        luke: human(id: 1000) {
          name
        }
        leia: human(id: 1003) {
          name
        }
      }
    |};

    let expected =
      okResponse(
        `Map([
          ("luke", `Map([("name", `String("Luke Skywalker"))])),
          ("leia", `Map([("name", `String("Leia Organa"))])),
        ]),
      );

    expectResponse(query, expected, assertion);
  });
});

describe("Uses fragments to express more complex queries", () => {
  testAsync("Allows us to query using duplicated content", assertion => {
    let query = {|
      query DuplicateFields {
        luke: human(id: 1000) {
          name
          homePlanet
        }
        leia: human(id: 1003) {
          name
          homePlanet
        }
      }
    |};

    let expected =
      okResponse(
        `Map([
          (
            "luke",
            `Map([("name", `String("Luke Skywalker")), ("homePlanet", `String("Tatooine"))]),
          ),
          (
            "leia",
            `Map([("name", `String("Leia Organa")), ("homePlanet", `String("Alderaan"))]),
          ),
        ]),
      );

    expectResponse(query, expected, assertion);
  });

  testAsync("Allows us to use a fragment to avoid duplicating content", assertion => {
    let query = {|
    query UseFragment {
      luke: human(id: 1000) {
        ...HumanFragment
      }
      leia: human(id: 1003) {
        ...HumanFragment
      }
    }

    fragment HumanFragment on Human {
      name
      homePlanet
    }
  |};

    let expected =
      okResponse(
        `Map([
          (
            "luke",
            `Map([("name", `String("Luke Skywalker")), ("homePlanet", `String("Tatooine"))]),
          ),
          (
            "leia",
            `Map([("name", `String("Leia Organa")), ("homePlanet", `String("Alderaan"))]),
          ),
        ]),
      );

    expectResponse(query, expected, assertion);
  });
});